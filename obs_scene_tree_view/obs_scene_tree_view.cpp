// Copyright (c) 2026 Anthony Mendez. All rights reserved.
// Use of this source code is governed by the GPL-2.0 license that can be
// found in the LICENSE file.

#include "obs_scene_tree_view/obs_scene_tree_view.h"

#include <QAction>
#include <QActionGroup>
#include <QLineEdit>
#include <QScrollBar>
#include <QTimer>
#include <QtWidgets/QComboBox>
#include <QtWidgets/QDialog>
#include <QtWidgets/QDockWidget>
#include <QtWidgets/QLabel>
#include <QtWidgets/QListWidget>
#include <QtWidgets/QMainWindow>
#include <QtWidgets/QMenu>
#include <QtWidgets/QMessageBox>
#include <QtWidgets/QPushButton>
#include <QtWidgets/QSpinBox>
#include <QtWidgets/QTextBrowser>
#include <QtWidgets/QVBoxLayout>
#include <QtWidgets/QWidgetAction>

#include <obs-frontend-api.h>
#include <obs-module.h>
#include <util/platform.h>

#include "obs_scene_tree_view/version.h"

namespace {

// Sets dynamic "class" and forces stylesheet recalculation on a QWidget.
void setClasses(QWidget *widget, const QString &newClasses) {
  if (!widget) {
    return;
  }
  if (widget->property("class").toString() != newClasses) {
    widget->setProperty("class", newClasses);
    /* Force style sheet recalculation. */
    QString qss = widget->styleSheet();
    widget->setStyleSheet("/* */");
    widget->setStyleSheet(qss);
  }
}

// Ensures disabled icons look identical to enabled by duplicating normal
// pixmaps into disabled states.
QIcon NonDimmedDisabled(const QIcon &src) {
  if (src.isNull()) {
    return src;
  }
  QIcon out;
  const QList<QSize> sizes = src.availableSizes(QIcon::Normal);
  if (sizes.isEmpty()) {
    return src;
  }
  for (const QSize &sz : sizes) {
    QPixmap pm = src.pixmap(sz, QIcon::Normal);
    out.addPixmap(pm, QIcon::Normal);
    out.addPixmap(pm, QIcon::Active);
    out.addPixmap(pm, QIcon::Disabled);
  }
  return out;
}

} // namespace

#define QT_TO_UTF8(str) str.toUtf8().constData()

namespace scene_tree_view {

ObsSceneTreeView::ObsSceneTreeView(QMainWindow *main_window)
    : QWidget(main_window),
      add_scene_act_(main_window->findChild<QAction *>("actionAddScene")),
      remove_scene_act_(main_window->findChild<QAction *>("actionRemoveScene")),
      toggle_toolbars_scene_act_(
          main_window->findChild<QAction *>("toggleListboxToolbars")) {
  config_t *const global_config = obs_frontend_get_user_config();
  config_set_default_bool(global_config, "SceneTreeView", "ShowSceneIcons",
                          false);
  config_set_default_bool(global_config, "SceneTreeView", "ShowFolderIcons",
                          false);

  assert(add_scene_act_);
  assert(remove_scene_act_);

  move_scene_up_act_ = main_window->findChild<QAction *>("actionSceneUp");
  move_scene_down_act_ = main_window->findChild<QAction *>("actionSceneDown");

  stv_dock_.setupUi(this);

  stv_dock_.stvAdd->setEnabled(false);
  stv_dock_.stvRemove->setEnabled(false);
  stv_dock_.stvAddFolder->setEnabled(false);
  stv_dock_.stvMoveUp->setEnabled(false);
  stv_dock_.stvMoveDown->setEnabled(false);

  stv_dock_.stvTree->SetItemModel(&scene_tree_items_);
  stv_dock_.stvTree->setDefaultDropAction(Qt::DropAction::MoveAction);

  stv_dock_.stvTree->setModel(&scene_tree_items_);

  // Initialize the loading label overlay.
  loading_label_ = new QLabel(stv_dock_.stvTree);
  loading_label_->setAlignment(Qt::AlignCenter);
  loading_label_->setStyleSheet("QLabel {"
                                "  font-size: 16px;"
                                "  color: palette(text);"
                                "  background: transparent;"
                                "}");
  loading_label_->setVisible(false);

  // Animation timer to cycle dots every 400ms.
  loading_timer_ = new QTimer(this);
  loading_timer_->setInterval(400);
  QObject::connect(loading_timer_, &QTimer::timeout, this, [this]() {
    loading_dots_count_ = (loading_dots_count_ + 1) % 4;
    QString dots = QString(loading_dots_count_, '.');
    const char *t = obs_module_text("SceneTreeView.Loading");
    QString base = QString::fromUtf8(t ? t : "Loading");
    loading_label_->setText(base + dots);
    // Keep label filling the tree viewport.
    loading_label_->setGeometry(stv_dock_.stvTree->viewport()->rect());
  });

  // Safeguard timeout (3 seconds) to force-hide loading if scenes never
  // resolve.
  loading_timeout_ = new QTimer(this);
  loading_timeout_->setSingleShot(true);
  loading_timeout_->setInterval(3000);
  QObject::connect(loading_timeout_, &QTimer::timeout, this,
                   [this]() { HideLoadingScreen(); });
  QObject::connect(&scene_tree_items_, &QStandardItemModel::rowsInserted, this,
                   [this](const QModelIndex &, int, int) {
                     if (scene_collection_name_) {
                       SaveSceneTree(scene_collection_name_);
                     }
                   });
  QObject::connect(&scene_tree_items_, &QStandardItemModel::rowsRemoved, this,
                   [this](const QModelIndex &, int, int) {
                     if (scene_collection_name_) {
                       SaveSceneTree(scene_collection_name_);
                     }
                   });
  if (auto sm = stv_dock_.stvTree->selectionModel()) {
    QObject::connect(sm, &QItemSelectionModel::currentChanged, this,
                     [this](const QModelIndex &, const QModelIndex &) {
                       UpdateMoveButtonsEnabled();
                     });
    QObject::connect(sm, &QItemSelectionModel::selectionChanged, this,
                     [this](const QItemSelection &, const QItemSelection &) {
                       UpdateMoveButtonsEnabled();
                     });
  }

  QObject::connect(stv_dock_.stvTree->verticalScrollBar(),
                   &QScrollBar::valueChanged, this, [this](int value) {
                     if (!is_loading_) {
                       last_v_scroll_ = value;
                     }
                   });
  QObject::connect(stv_dock_.stvTree->horizontalScrollBar(),
                   &QScrollBar::valueChanged, this, [this](int value) {
                     if (!is_loading_) {
                       last_h_scroll_ = value;
                     }
                   });
  QObject::connect(stv_dock_.stvTree, &QTreeView::expanded, this,
                   [this](const QModelIndex &index) {
                     QStandardItem *item =
                         scene_tree_items_.itemFromIndex(index);
                     if (item) {
                       blog(LOG_INFO, "[%s] EVENT: expanded item '%s', type %d",
                            obs_module_name(),
                            item->text().toStdString().c_str(), item->type());
                       if (item->type() == StvItemModel::kFolder) {
                         item->setData(true, StvItemModel::kExpanded);
                       }
                     }
                     if (!is_loading_ && scene_collection_name_) {
                       SaveSceneTree(scene_collection_name_);
                     }
                   });
  QObject::connect(
      stv_dock_.stvTree, &QTreeView::collapsed, this,
      [this](const QModelIndex &index) {
        QStandardItem *item = scene_tree_items_.itemFromIndex(index);
        if (item) {
          blog(LOG_INFO, "[%s] EVENT: collapsed item '%s', type %d",
               obs_module_name(), item->text().toStdString().c_str(),
               item->type());
          if (item->type() == StvItemModel::kFolder) {
            item->setData(false, StvItemModel::kExpanded);
          }
        }
        if (!is_loading_ && scene_collection_name_) {
          SaveSceneTree(scene_collection_name_);
        }
      });

  const bool show_icons =
      config_get_bool(global_config, "BasicWindow", "ShowListboxToolbars");
  on_toggleListboxToolbars(show_icons);

  // Add callbacks to OBS frontend events.
  obs_frontend_add_event_callback(&ObsSceneTreeView::obs_frontend_event_cb,
                                  this);
  obs_frontend_add_save_callback(&ObsSceneTreeView::obs_frontend_save_cb, this);

  if (add_scene_act_) {
    QObject::connect(stv_dock_.stvAdd, &QToolButton::released, add_scene_act_,
                     &QAction::trigger);
  } else {
    stv_dock_.stvAdd->setEnabled(false);
  }

  QObject::connect(
      stv_dock_.stvTree->itemDelegate(),
      SIGNAL(closeEditor(QWidget *, QAbstractItemDelegate::EndEditHint)), this,
      SLOT(on_SceneNameEdited(QWidget *)));

  QObject::connect(toggle_toolbars_scene_act_, &QAction::triggered, this,
                   &ObsSceneTreeView::on_toggleListboxToolbars);

  UpdateMoveButtonsEnabled();
}

ObsSceneTreeView::~ObsSceneTreeView() {
  obs_frontend_remove_save_callback(&ObsSceneTreeView::obs_frontend_save_cb,
                                    this);
  obs_frontend_remove_event_callback(&ObsSceneTreeView::obs_frontend_event_cb,
                                     this);
}

void ObsSceneTreeView::SaveSceneTree(const char *scene_collection) {
  if (!scene_collection) {
    blog(LOG_INFO, "[%s] SaveSceneTree: skipped (null collection)",
         obs_module_name());
    return;
  }

  int item_count = scene_tree_items_.invisibleRootItem()->rowCount();
  blog(LOG_INFO,
       "[%s] SaveSceneTree: collection='%s', root_items=%d, pending=%s",
       obs_module_name(), scene_collection, item_count,
       HasUnassociatedScenes() ? "true" : "false");

  BPtr<char> stv_config_file_path =
      obs_module_config_path(kSceneTreeConfigFile.data());

  OBSDataAutoRelease stv_data =
      obs_data_create_from_json_file(stv_config_file_path);
  if (!stv_data) {
    stv_data = obs_data_create();
  }

  scene_tree_items_.SaveSceneTree(stv_data, scene_collection,
                                  stv_dock_.stvTree);

  std::string v_scroll_key = std::string(scene_collection) + "_v_scroll";
  std::string h_scroll_key = std::string(scene_collection) + "_h_scroll";
  obs_data_set_int(stv_data, v_scroll_key.c_str(), last_v_scroll_);
  obs_data_set_int(stv_data, h_scroll_key.c_str(), last_h_scroll_);

  if (!obs_data_save_json(stv_data, stv_config_file_path)) {
    blog(LOG_WARNING, "[%s] Failed to save scene tree in '%s'",
         obs_module_name(), stv_config_file_path.Get());
  }
}

void ObsSceneTreeView::LoadSceneTree(const char *scene_collection) {
  assert(scene_collection);
  blog(LOG_INFO, "[%s] LoadSceneTree: collection='%s'", obs_module_name(),
       scene_collection);

  is_loading_ = true;

  BPtr<char> stv_config_file_path =
      obs_module_config_path(kSceneTreeConfigFile.data());

  OBSDataAutoRelease stv_data =
      obs_data_create_from_json_file(stv_config_file_path);
  scene_tree_items_.blockSignals(true);
  scene_tree_items_.LoadSceneTree(stv_data, scene_collection);
  scene_tree_items_.blockSignals(false);
  scene_tree_items_.layoutChanged();

  // Load and restore scroll positions
  std::string v_scroll_key = std::string(scene_collection) + "_v_scroll";
  std::string h_scroll_key = std::string(scene_collection) + "_h_scroll";
  last_v_scroll_ = (int)obs_data_get_int(stv_data, v_scroll_key.c_str());
  last_h_scroll_ = (int)obs_data_get_int(stv_data, h_scroll_key.c_str());

  QTimer::singleShot(100, this, [this]() {
    bool was_loading = is_loading_;
    is_loading_ = true;
    RestoreExpansionStates();
    stv_dock_.stvTree->verticalScrollBar()->setValue(last_v_scroll_);
    stv_dock_.stvTree->horizontalScrollBar()->setValue(last_h_scroll_);
    is_loading_ = was_loading;
  });

  int item_count = scene_tree_items_.invisibleRootItem()->rowCount();
  bool pending = HasUnassociatedScenes();
  blog(LOG_INFO, "[%s] LoadSceneTree: loaded %d root items, pending=%s",
       obs_module_name(), item_count, pending ? "true" : "false");

  // Show loading screen if there are unresolved placeholder scenes.
  if (pending) {
    ShowLoadingScreen();
  }
}

void ObsSceneTreeView::UpdateTreeView() {
  obs_frontend_source_list scene_list = {};
  obs_frontend_get_scenes(&scene_list);

  blog(LOG_INFO, "[%s] UpdateTreeView: scene_count=%zu", obs_module_name(),
       scene_list.sources.num);

  scene_tree_items_.blockSignals(true);
  scene_tree_items_.UpdateTree(scene_list, stv_dock_.stvTree->currentIndex());
  scene_tree_items_.blockSignals(false);
  scene_tree_items_.layoutChanged();

  obs_frontend_source_list_free(&scene_list);

  bool was_loading = is_loading_;
  is_loading_ = true;
  RestoreExpansionStates();
  is_loading_ = was_loading;

  int item_count = scene_tree_items_.invisibleRootItem()->rowCount();
  bool pending = HasUnassociatedScenes();
  blog(LOG_INFO, "[%s] UpdateTreeView: root_items=%d, pending=%s",
       obs_module_name(), item_count, pending ? "true" : "false");

  // Hide loading screen once all placeholders are resolved.
  if (!pending) {
    HideLoadingScreen();
  }

  SaveSceneTree(scene_collection_name_);
}

void ObsSceneTreeView::SelectCurrentScene() {
  QStandardItem *item = scene_tree_items_.GetCurrentSceneItem();
  if (item && item->index() != stv_dock_.stvTree->currentIndex()) {
    QMetaObject::invokeMethod(stv_dock_.stvTree, "setCurrentIndex",
                              Q_ARG(QModelIndex, item->index()));
  }
}

void ObsSceneTreeView::ApplyThemeAndIcons() {
  QMainWindow *main_window =
      reinterpret_cast<QMainWindow *>(obs_frontend_get_main_window());
  if (add_scene_act_) {
    stv_dock_.stvAdd->setIcon(add_scene_act_->icon());
  }
  if (remove_scene_act_) {
    stv_dock_.stvRemove->setIcon(remove_scene_act_->icon());
  }
  stv_dock_.stvAddFolder->setIcon(
      main_window->property("groupIcon").value<QIcon>());

  if (move_scene_up_act_) {
    stv_dock_.stvMoveUp->setIcon(NonDimmedDisabled(move_scene_up_act_->icon()));
  }
  if (move_scene_down_act_) {
    stv_dock_.stvMoveDown->setIcon(
        NonDimmedDisabled(move_scene_down_act_->icon()));
  }

  auto copyProps = [](QAction *act, QWidget *w) {
    if (!act || !w) {
      return;
    }
    const auto names = act->dynamicPropertyNames();
    for (const QByteArray &n : names) {
      w->setProperty(n.constData(), act->property(n.constData()));
    }
  };
  copyProps(add_scene_act_, stv_dock_.stvAdd);
  copyProps(remove_scene_act_, stv_dock_.stvRemove);

  QString addCls =
      add_scene_act_ ? add_scene_act_->property("class").toString() : QString();

  copyProps(move_scene_up_act_, stv_dock_.stvMoveUp);
  copyProps(move_scene_down_act_, stv_dock_.stvMoveDown);

  QString remCls = remove_scene_act_
                       ? remove_scene_act_->property("class").toString()
                       : QString();

  QString addClasses = QStringLiteral("btn-tool");
  if (!addCls.isEmpty()) {
    addClasses += QStringLiteral(" ") + addCls;
  } else {
    addClasses += QStringLiteral(" icon-plus");
  }
  QString removeClasses = QStringLiteral("btn-tool");
  if (!remCls.isEmpty()) {
    removeClasses += QStringLiteral(" ") + remCls;
  } else {
    removeClasses += QStringLiteral(" icon-trash");
  }

  QString upCls = move_scene_up_act_
                      ? move_scene_up_act_->property("class").toString()
                      : QString();
  QString downCls = move_scene_down_act_
                        ? move_scene_down_act_->property("class").toString()
                        : QString();

  QString upClasses = QStringLiteral("btn-tool");
  if (!upCls.isEmpty()) {
    upClasses += QStringLiteral(" ") + upCls;
  } else {
    upClasses += QStringLiteral(" icon-up");
  }
  QString downClasses = QStringLiteral("btn-tool");
  if (!downCls.isEmpty()) {
    downClasses += QStringLiteral(" ") + downCls;
  } else {
    downClasses += QStringLiteral(" icon-down");
  }

  setClasses(stv_dock_.stvAdd, addClasses);
  setClasses(stv_dock_.stvRemove, removeClasses);
  setClasses(stv_dock_.stvAddFolder, QStringLiteral("btn-tool"));

  setClasses(stv_dock_.stvMoveUp, upClasses);
  setClasses(stv_dock_.stvMoveDown, downClasses);

  QString qss = styleSheet();
  setStyleSheet("/* */");
  setStyleSheet(qss);
}

void ObsSceneTreeView::ObsFrontendEvent(enum obs_frontend_event event) {
  if (event == OBS_FRONTEND_EVENT_FINISHED_LOADING) {
    if (!g_stv_added && g_stv_dock) {
      const char *t = obs_module_text("SceneTreeView.Title");
      QString title = QString::fromUtf8(t ? t : "");
      if (title.isEmpty() || title == QLatin1String("SceneTreeView.Title")) {
        title = QStringLiteral("Scene Tree View");
      }

      g_stv_added = obs_frontend_add_dock_by_id("obs_scene_tree_view",
                                                QT_TO_UTF8(title), g_stv_dock);
      if (g_stv_added) {
        blog(LOG_INFO, "[%s] retry add_dock_by_id succeeded",
             obs_module_name());
      } else {
        blog(LOG_ERROR, "[%s] retry add_dock_by_id failed", obs_module_name());
      }
    }

    scene_collection_name_ = obs_frontend_get_current_scene_collection();

    LoadSceneTree(scene_collection_name_);
    UpdateTreeView();

    SelectCurrentScene();

    ApplyThemeAndIcons();

    stv_dock_.stvAdd->setEnabled(true);
    stv_dock_.stvRemove->setEnabled(true);
    stv_dock_.stvAddFolder->setEnabled(true);

    stv_dock_.stvMoveUp->setEnabled(true);
    stv_dock_.stvMoveDown->setEnabled(true);
    UpdateMoveButtonsEnabled();

    static bool tools_menu_added = false;
    if (!tools_menu_added) {
      tools_menu_added = true;
      QAction *about_action = static_cast<QAction *>(
          obs_frontend_add_tools_menu_qaction(obs_module_name()));
      if (about_action) {
        connect(about_action, &QAction::triggered, this, [this]() {
          QMainWindow *main_window =
              reinterpret_cast<QMainWindow *>(obs_frontend_get_main_window());
          QMessageBox mb(main_window);
          mb.setWindowTitle(obs_module_name());
          mb.setTextFormat(Qt::RichText);
          mb.setTextInteractionFlags(Qt::TextBrowserInteraction);
          mb.setText(QString("<h3>%1</h3>"
                             "<p><b>Version:</b> %2</p>"
                             "<p><b>Authors:</b> %3</p>"
                             "<p><b>GitHub:</b> <a href=\"%4\">%4</a></p>")
                         .arg(obs_module_name())
                         .arg(PROJECT_VERSION)
                         .arg(PROJECT_CONTRIBUTORS)
                         .arg(PROJECT_WEBSITE));

          QPushButton *history_btn =
              mb.addButton(obs_module_text("SceneTreeView.VersionHistory"),
                           QMessageBox::ActionRole);
          mb.addButton(QMessageBox::Ok);
          mb.exec();

          if (mb.clickedButton() == history_btn) {
            QDialog history_dialog(main_window);
            history_dialog.setWindowTitle(QString::fromUtf8(obs_module_name()) +
                                          QStringLiteral(" - ") +
                                          QString::fromUtf8(obs_module_text(
                                              "SceneTreeView.VersionHistory")));
            QVBoxLayout *layout = new QVBoxLayout(&history_dialog);
            QTextBrowser *browser = new QTextBrowser(&history_dialog);
            browser->setOpenExternalLinks(true);
            browser->setMarkdown(QString::fromUtf8(PROJECT_VERSION_HISTORY));
            layout->addWidget(browser);
            history_dialog.resize(550, 450);
            history_dialog.exec();
          }
        });
      }
    }

  } else if (event == OBS_FRONTEND_EVENT_THEME_CHANGED) {
    ApplyThemeAndIcons();

  } else if (event == OBS_FRONTEND_EVENT_SCENE_LIST_CHANGED) {
    UpdateTreeView();
  } else if (event == OBS_FRONTEND_EVENT_SCENE_CHANGED ||
             event == OBS_FRONTEND_EVENT_PREVIEW_SCENE_CHANGED) {
    SelectCurrentScene();
  } else if (event == OBS_FRONTEND_EVENT_SCENE_COLLECTION_CLEANUP) {
    blog(LOG_INFO, "[%s] EVENT: SCENE_COLLECTION_CLEANUP", obs_module_name());
    // Null the name BEFORE cleanup so that rowsRemoved signals
    // don't trigger SaveSceneTree with an empty tree.
    scene_collection_name_ = nullptr;
    scene_tree_items_.CleanupSceneTree();
  } else if (event == OBS_FRONTEND_EVENT_SCENE_COLLECTION_CHANGING) {
    blog(LOG_INFO, "[%s] EVENT: SCENE_COLLECTION_CHANGING (saving '%s')",
         obs_module_name(),
         scene_collection_name_ ? scene_collection_name_.Get() : "(null)");
    SaveSceneTree(scene_collection_name_);
  } else if (event == OBS_FRONTEND_EVENT_SCENE_COLLECTION_CHANGED) {
    scene_collection_name_ = obs_frontend_get_current_scene_collection();
    blog(LOG_INFO, "[%s] EVENT: SCENE_COLLECTION_CHANGED to '%s'",
         obs_module_name(),
         scene_collection_name_ ? scene_collection_name_.Get() : "(null)");
    LoadSceneTree(scene_collection_name_);
    UpdateTreeView();
  } else if (event == OBS_FRONTEND_EVENT_SCENE_COLLECTION_RENAMED) {
    scene_collection_name_ = obs_frontend_get_current_scene_collection();
    blog(LOG_INFO, "[%s] EVENT: SCENE_COLLECTION_RENAMED to '%s'",
         obs_module_name(),
         scene_collection_name_ ? scene_collection_name_.Get() : "(null)");
    SaveSceneTree(scene_collection_name_);
    UpdateTreeView();
  }
}

void ObsSceneTreeView::ObsFrontendSave(obs_data_t * /*save_data*/,
                                       bool saving) {
  if (saving) {
    SaveSceneTree(scene_collection_name_);
  }
}

void ObsSceneTreeView::UpdateMoveButtonsEnabled() {
  bool enableUp = false;
  bool enableDown = false;
  const QModelIndex idx = stv_dock_.stvTree->currentIndex();
  if (idx.isValid()) {
    QStandardItem *parent = scene_tree_items_.GetParentOrRoot(idx);
    const int row = idx.row();
    const int count = parent ? parent->rowCount() : 0;
    enableUp = (row > 0);
    enableDown = (row >= 0 && row < count - 1);
  }
  stv_dock_.stvMoveUp->setEnabled(enableUp);
  stv_dock_.stvMoveDown->setEnabled(enableDown);
}

void ObsSceneTreeView::ShowLoadingScreen() {
  stv_dock_.stvTree->setVisible(false);
  loading_label_->setParent(stv_dock_.frame);
  loading_label_->setGeometry(stv_dock_.frame->rect());
  loading_label_->setVisible(true);
  loading_dots_count_ = 0;
  const char *t = obs_module_text("SceneTreeView.Loading");
  loading_label_->setText(QString::fromUtf8(t ? t : "Loading"));
  loading_timer_->start();
  loading_timeout_->start();
}

void ObsSceneTreeView::HideLoadingScreen() {
  loading_timer_->stop();
  loading_timeout_->stop();
  loading_label_->setVisible(false);
  loading_label_->setParent(stv_dock_.stvTree);
  stv_dock_.stvTree->setVisible(true);

  QTimer::singleShot(50, this, [this]() {
    bool was_loading = is_loading_;
    is_loading_ = true;
    RestoreExpansionStates();
    is_loading_ = was_loading;
  });
}

bool ObsSceneTreeView::HasUnassociatedScenes() const {
  return HasUnassociatedScenesHelper(scene_tree_items_.invisibleRootItem());
}

bool ObsSceneTreeView::HasUnassociatedScenesHelper(
    QStandardItem *parent) const {
  if (!parent) {
    return false;
  }
  for (int i = 0; i < parent->rowCount(); ++i) {
    QStandardItem *child = parent->child(i);
    if (!child) {
      continue;
    }
    if (child->type() == StvItemModel::kScene) {
      obs_weak_source_t *weak =
          child->data(StvItemModel::kObsScene).value<ObsWeakSourcePtr>().ptr;
      if (weak == nullptr) {
        return true;
      }
    } else if (child->type() == StvItemModel::kFolder) {
      if (HasUnassociatedScenesHelper(child)) {
        return true;
      }
    }
  }
  return false;
}

void ObsSceneTreeView::RestoreExpansionStates(QStandardItem *parent) {
  if (!parent) {
    parent = scene_tree_items_.invisibleRootItem();
  }
  for (int i = 0; i < parent->rowCount(); ++i) {
    QStandardItem *child = parent->child(i);
    if (child && child->type() == StvItemModel::kFolder) {
      bool expanded = child->data(StvItemModel::kExpanded).toBool();
      blog(
          LOG_INFO,
          "[%s] RestoreExpansionStates: restoring folder '%s' to expanded = %s",
          obs_module_name(), child->text().toStdString().c_str(),
          expanded ? "true" : "false");
      stv_dock_.stvTree->setExpanded(child->index(), expanded);
      RestoreExpansionStates(child);
    }
  }
}

} // namespace scene_tree_view
