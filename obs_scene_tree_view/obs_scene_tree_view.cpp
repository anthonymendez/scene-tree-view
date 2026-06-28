// Copyright (c) 2026 Anthony Mendez. All rights reserved.
// Use of this source code is governed by the GPL-2.0 license that can be
// found in the LICENSE file.

#include "obs_scene_tree_view/obs_scene_tree_view.h"

#include <QAction>
#include <QLineEdit>
#include <QtWidgets/QComboBox>
#include <QtWidgets/QDialog>
#include <QtWidgets/QDockWidget>
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

OBS_DECLARE_MODULE();
OBS_MODULE_AUTHOR(PROJECT_AUTHOR);
OBS_MODULE_USE_DEFAULT_LOCALE(PROJECT_DATA_FOLDER, "en-US");

namespace {

// Global dock pointer and registration status for retry logic.
scene_tree_view::ObsSceneTreeView* g_stv_dock = nullptr;
bool g_stv_added = false;

// Sets dynamic "class" and forces stylesheet recalculation on a QWidget.
void setClasses(QWidget* widget, const QString& newClasses) {
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

// Ensures disabled icons look identical to enabled by duplicating normal pixmaps
// into disabled states.
QIcon NonDimmedDisabled(const QIcon& src) {
  if (src.isNull()) {
    return src;
  }
  QIcon out;
  const QList<QSize> sizes = src.availableSizes(QIcon::Normal);
  if (sizes.isEmpty()) {
    return src;
  }
  for (const QSize& sz : sizes) {
    QPixmap pm = src.pixmap(sz, QIcon::Normal);
    out.addPixmap(pm, QIcon::Normal);
    out.addPixmap(pm, QIcon::Active);
    out.addPixmap(pm, QIcon::Disabled);
  }
  return out;
}

}  // namespace

MODULE_EXPORT const char* obs_module_description(void) {
  return obs_module_text("Description");
}

MODULE_EXPORT const char* obs_module_name(void) {
  return obs_module_text("SceneTreeView");
}

#define QT_UTF8(str) QString::fromUtf8(str)
#define QT_TO_UTF8(str) str.toUtf8().constData()

MODULE_EXPORT bool obs_module_load(void) {
  blog(LOG_INFO, "[%s] loaded version %s", obs_module_name(), PROJECT_VERSION);

  BPtr<char> stv_config_path = obs_module_config_path("");
  if (!os_mkdir(stv_config_path)) {
    blog(LOG_WARNING, "[%s] failed to create config dir '%s'",
         obs_module_name(), stv_config_path.Get());
  }

  QMainWindow* main_window =
      reinterpret_cast<QMainWindow*>(obs_frontend_get_main_window());
  obs_frontend_push_ui_translation(obs_module_get_string);

  scene_tree_view::ObsSceneTreeView* view =
      new scene_tree_view::ObsSceneTreeView(main_window);
  view->setObjectName("obs_scene_tree_view");

  const char* t = obs_module_text("SceneTreeView.Title");
  QString title = QString::fromUtf8(t ? t : "");
  if (title.isEmpty() || title == QLatin1String("SceneTreeView.Title")) {
    title = QStringLiteral("Scene Tree View");
  }

  bool added = obs_frontend_add_dock_by_id("obs_scene_tree_view",
                                           QT_TO_UTF8(title), view);

  if (added) {
    blog(LOG_INFO, "[%s] registered via add_dock_by_id", obs_module_name());

    // Configure dock features on the OBS-created QDockWidget wrapper.
    if (auto* dockWidget = qobject_cast<QDockWidget*>(view->parentWidget())) {
      dockWidget->setAllowedAreas(Qt::AllDockWidgetAreas);
      dockWidget->setFeatures(QDockWidget::DockWidgetClosable |
                              QDockWidget::DockWidgetMovable |
                              QDockWidget::DockWidgetFloatable);
    }
  } else {
    blog(LOG_ERROR, "[%s] failed to register dock via add_dock_by_id",
         obs_module_name());
  }

  g_stv_dock = view;
  g_stv_added = added;
  obs_frontend_pop_ui_translation();

  return true;
}

MODULE_EXPORT void obs_module_unload() {}

namespace scene_tree_view {

ObsSceneTreeView::ObsSceneTreeView(QMainWindow* main_window)
    : QWidget(main_window),
      add_scene_act_(main_window->findChild<QAction*>("actionAddScene")),
      remove_scene_act_(main_window->findChild<QAction*>("actionRemoveScene")),
      toggle_toolbars_scene_act_(
          main_window->findChild<QAction*>("toggleListboxToolbars")) {
  config_t* const global_config = obs_frontend_get_user_config();
  config_set_default_bool(global_config, "SceneTreeView", "ShowSceneIcons",
                          false);
  config_set_default_bool(global_config, "SceneTreeView", "ShowFolderIcons",
                          false);

  assert(add_scene_act_);
  assert(remove_scene_act_);

  move_scene_up_act_ = main_window->findChild<QAction*>("actionSceneUp");
  move_scene_down_act_ = main_window->findChild<QAction*>("actionSceneDown");

  stv_dock_.setupUi(this);

  stv_dock_.stvAdd->setEnabled(false);
  stv_dock_.stvRemove->setEnabled(false);
  stv_dock_.stvAddFolder->setEnabled(false);
  stv_dock_.stvMoveUp->setEnabled(false);
  stv_dock_.stvMoveDown->setEnabled(false);

  stv_dock_.stvTree->SetItemModel(&scene_tree_items_);
  stv_dock_.stvTree->setDefaultDropAction(Qt::DropAction::MoveAction);

  stv_dock_.stvTree->setModel(&scene_tree_items_);
  if (auto sm = stv_dock_.stvTree->selectionModel()) {
    QObject::connect(sm, &QItemSelectionModel::currentChanged, this,
                     [this](const QModelIndex&, const QModelIndex&) {
                       UpdateMoveButtonsEnabled();
                     });
    QObject::connect(sm, &QItemSelectionModel::selectionChanged, this,
                     [this](const QItemSelection&, const QItemSelection&) {
                       UpdateMoveButtonsEnabled();
                     });
  }

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
      SIGNAL(closeEditor(QWidget*, QAbstractItemDelegate::EndEditHint)), this,
      SLOT(on_SceneNameEdited(QWidget*)));

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

void ObsSceneTreeView::SaveSceneTree(const char* scene_collection) {
  if (!scene_collection) {
    return;
  }

  BPtr<char> stv_config_file_path =
      obs_module_config_path(kSceneTreeConfigFile.data());

  OBSDataAutoRelease stv_data =
      obs_data_create_from_json_file(stv_config_file_path);
  if (!stv_data) {
    stv_data = obs_data_create();
  }

  scene_tree_items_.SaveSceneTree(stv_data, scene_collection,
                                  stv_dock_.stvTree);

  if (!obs_data_save_json(stv_data, stv_config_file_path)) {
    blog(LOG_WARNING, "[%s] Failed to save scene tree in '%s'",
         obs_module_name(), stv_config_file_path.Get());
  }
}

void ObsSceneTreeView::LoadSceneTree(const char* scene_collection) {
  assert(scene_collection);

  BPtr<char> stv_config_file_path =
      obs_module_config_path(kSceneTreeConfigFile.data());

  OBSDataAutoRelease stv_data =
      obs_data_create_from_json_file(stv_config_file_path);
  scene_tree_items_.LoadSceneTree(stv_data, scene_collection,
                                  stv_dock_.stvTree);
}

void ObsSceneTreeView::UpdateTreeView() {
  obs_frontend_source_list scene_list = {};
  obs_frontend_get_scenes(&scene_list);

  scene_tree_items_.UpdateTree(scene_list, stv_dock_.stvTree->currentIndex());

  obs_frontend_source_list_free(&scene_list);

  SaveSceneTree(scene_collection_name_);
}

void ObsSceneTreeView::on_toggleListboxToolbars(bool visible) {
  stv_dock_.listbox->setVisible(visible);
}

void ObsSceneTreeView::on_stvAddFolder_clicked() {
  int row;
  QStandardItem* selected =
      scene_tree_items_.itemFromIndex(stv_dock_.stvTree->currentIndex());
  if (!selected) {
    selected = scene_tree_items_.invisibleRootItem();
    row = selected->rowCount();
  } else {
    if (selected->type() == StvItemModel::kFolder) {
      row = selected->rowCount();
    } else {
      row = selected->row() + 1;
      selected = scene_tree_items_.GetParentOrRoot(selected->index());
    }
  }

  QString format{obs_module_text("SceneTreeView.DefaultFolderName")};
  if (!format.contains("%1")) {
    format = QStringLiteral("Folder %1");
  }

  int i = 1;
  QString new_folder_name = format.arg(i);
  while (!scene_tree_items_.CheckFolderNameUniqueness(new_folder_name,
                                                      selected)) {
    new_folder_name = format.arg(++i);
  }

  StvFolderItem* pItem = new StvFolderItem(new_folder_name);
  selected->insertRow(row, pItem);

  SaveSceneTree(scene_collection_name_);
}

void ObsSceneTreeView::on_stvRemove_released() {
  QStandardItem* selected =
      scene_tree_items_.itemFromIndex(stv_dock_.stvTree->currentIndex());
  if (selected) {
    assert(selected->type() == StvItemModel::kFolder ||
           selected->type() == StvItemModel::kScene);
    if (selected->type() == StvItemModel::kScene) {
      QMetaObject::invokeMethod(remove_scene_act_, "triggered");
    } else {
      RemoveFolder(selected);
    }
  }
}

void ObsSceneTreeView::on_stvTree_customContextMenuRequested(const QPoint& pos) {
  QStandardItem* item =
      scene_tree_items_.itemFromIndex(stv_dock_.stvTree->indexAt(pos));

  QMainWindow* main_window =
      reinterpret_cast<QMainWindow*>(obs_frontend_get_main_window());

  QMenu popup(this);

  popup.addAction(obs_module_text("SceneTreeView.AddScene"), main_window,
                  SLOT(on_actionAddScene_triggered()));

  popup.addAction(obs_module_text("SceneTreeView.AddFolder"), this,
                  SLOT(on_stvAddFolder_clicked()));

  if (item) {
    if (item->type() == StvItemModel::kScene) {
      QAction* copyFilters = new QAction(QTStr("Copy.Filters"), this);
      copyFilters->setEnabled(false);
      connect(copyFilters, SIGNAL(triggered()), main_window,
              SLOT(SceneCopyFilters()));
      QAction* pasteFilters = new QAction(QTStr("Paste.Filters"), this);
      connect(pasteFilters, SIGNAL(triggered()), main_window,
              SLOT(ScenePasteFilters()));

      popup.addSeparator();
      popup.addAction(QTStr("Duplicate"), main_window,
                      SLOT(DuplicateSelectedScene()));
      popup.addAction(copyFilters);
      popup.addAction(pasteFilters);
      popup.addSeparator();
      QAction* rename = popup.addAction(QTStr("Rename"));
      QObject::connect(rename, SIGNAL(triggered()), stv_dock_.stvTree,
                       SLOT(EditSelectedItem()));
      popup.addAction(QTStr("Remove"), main_window,
                      SLOT(RemoveSelectedScene()));
      popup.addSeparator();

      QAction* sceneWindow = popup.addAction(QTStr("SceneWindow"), main_window,
                                             SLOT(OpenSceneWindow()));

      popup.addAction(sceneWindow);
      popup.addAction(QTStr("Screenshot.Scene"), main_window,
                      SLOT(ScreenshotScene()));
      popup.addSeparator();
      popup.addAction(QTStr("Filters"), main_window, SLOT(OpenSceneFilters()));

      popup.addSeparator();

      per_scene_transition_menu_.reset(
          CreatePerSceneTransitionMenu(main_window));
      popup.addMenu(per_scene_transition_menu_.get());

      popup.addSeparator();

      QAction* multiviewAction = popup.addAction(QTStr("ShowInMultiview"));

      OBSSourceAutoRelease source = scene_tree_items_.GetCurrentScene();
      OBSDataAutoRelease sceneSettings =
          obs_source_get_private_settings(source);

      obs_data_set_default_bool(sceneSettings, "show_in_multiview", true);
      bool show = obs_data_get_bool(sceneSettings, "show_in_multiview");

      multiviewAction->setCheckable(true);
      multiviewAction->setChecked(show);

      auto showInMultiview = [main_window](OBSData settings) {
        bool show = obs_data_get_bool(settings, "show_in_multiview");
        obs_data_set_bool(settings, "show_in_multiview", !show);
        QMetaObject::invokeMethod(main_window, "ScenesReordered");
      };

      connect(multiviewAction, &QAction::triggered,
              std::bind(showInMultiview, sceneSettings.Get()));

      copyFilters->setEnabled(obs_source_filter_count(source) > 0);
    }

    popup.addSeparator();

    // Enable/disable scene or folder icon.
    const auto toggleName =
        item->type() == StvItemModel::kScene
            ? obs_module_text("SceneTreeView.ToggleSceneIcons")
            : obs_module_text("SceneTreeView.ToggleFolderIcons");

    QAction* toggleIconAction = popup.addAction(toggleName);
    toggleIconAction->setCheckable(true);

    const auto configName = item->type() == StvItemModel::kScene
                                ? "ShowSceneIcons"
                                : "ShowFolderIcons";
    const bool showIcon = config_get_bool(obs_frontend_get_user_config(),
                                          "SceneTreeView", configName);

    toggleIconAction->setChecked(showIcon);

    auto toggleIcon = [this, showIcon, configName, item]() {
      config_set_bool(obs_frontend_get_user_config(), "SceneTreeView",
                      configName, !showIcon);
      scene_tree_items_.SetIconVisibility(
          !showIcon, (StvItemModel::QItemType)item->type());
    };

    connect(toggleIconAction, &QAction::triggered, toggleIcon);
  }

  popup.exec(QCursor::pos());
}

void ObsSceneTreeView::on_SceneNameEdited(QWidget* editor) {
  QStandardItem* selected =
      scene_tree_items_.itemFromIndex(stv_dock_.stvTree->currentIndex());
  if (selected->type() == StvItemModel::kScene) {
    QMainWindow* main_window =
        reinterpret_cast<QMainWindow*>(obs_frontend_get_main_window());
    QMetaObject::invokeMethod(main_window, "SceneNameEdited",
                              Q_ARG(QWidget*, editor));
  } else {
    QLineEdit* edit = qobject_cast<QLineEdit*>(editor);
    std::string text = QT_TO_UTF8(edit->text().trimmed());

    selected->setText(scene_tree_items_.CreateUniqueFolderName(
        selected, scene_tree_items_.GetParentOrRoot(selected->index())));
  }
}

void ObsSceneTreeView::on_stvTree_customContextMenuRequested(
    const QPoint &pos) {
  QStandardItem *item = this->_scene_tree_items.itemFromIndex(
      this->_stv_dock.stvTree->indexAt(pos));

  QMainWindow *main_window =
      reinterpret_cast<QMainWindow *>(obs_frontend_get_main_window());

  QMenu popup(this);
  //	QMenu order(QTStr("Basic.MainMenu.Edit.Order"), this);

  popup.addAction(obs_module_text("SceneTreeView.AddScene"), main_window,
                  SLOT(on_actionAddScene_triggered()));

  popup.addAction(obs_module_text("SceneTreeView.AddFolder"), this,
                  SLOT(on_stvAddFolder_clicked()));

  if (item) {
    if (item->type() == StvItemModel::SCENE) {
      QAction *copyFilters = new QAction(QTStr("Copy.Filters"), this);
      copyFilters->setEnabled(false);
      connect(copyFilters, SIGNAL(triggered()), main_window,
              SLOT(SceneCopyFilters()));
      QAction *pasteFilters = new QAction(QTStr("Paste.Filters"), this);
      //			pasteFilters->setEnabled(
      //			    !obs_weak_source_expired(copyFiltersSource));
      //// Cannot use (we can't check copyFiltersSource, as it's a private
      // member of OBSBasic)
      connect(pasteFilters, SIGNAL(triggered()), main_window,
              SLOT(ScenePasteFilters()));

      popup.addSeparator();
      popup.addAction(QTStr("Duplicate"), main_window,
                      SLOT(DuplicateSelectedScene()));
      popup.addAction(copyFilters);
      popup.addAction(pasteFilters);
      popup.addSeparator();
      QAction *rename = popup.addAction(QTStr("Rename"));
      QObject::connect(rename, SIGNAL(triggered()), this->_stv_dock.stvTree,
                       SLOT(EditSelectedItem()));
      popup.addAction(QTStr("Remove"), main_window,
                      SLOT(RemoveSelectedScene()));
      popup.addSeparator();

      //			order.addAction(QTStr("Basic.MainMenu.Edit.Order.MoveUp"),
      //			                main_window,
      // SLOT(on_actionSceneUp_triggered()));
      //			order.addAction(QTStr("Basic.MainMenu.Edit.Order.MoveDown"),
      //			        this,
      // SLOT(on_actionSceneDown_triggered()));
      // order.addSeparator();

      //			order.addAction(QTStr("Basic.MainMenu.Edit.Order.MoveToTop"),
      //			        this, SLOT(MoveSceneToTop()));
      //			order.addAction(QTStr("Basic.MainMenu.Edit.Order.MoveToBottom"),
      //				    this, SLOT(MoveSceneToBottom()));
      //			popup.addMenu(&order);

      //			popup.addSeparator();

      //			delete sceneProjectorMenu;
      //			sceneProjectorMenu = new
      // QMenu(QTStr("SceneProjector"));
      //			AddProjectorMenuMonitors(sceneProjectorMenu,
      // this, 				         SLOT(OpenSceneProjector()));
      // popup.addMenu(sceneProjectorMenu);

      QAction *sceneWindow = popup.addAction(QTStr("SceneWindow"), main_window,
                                             SLOT(OpenSceneWindow()));

      popup.addAction(sceneWindow);
      popup.addAction(QTStr("Screenshot.Scene"), main_window,
                      SLOT(ScreenshotScene()));
      popup.addSeparator();
      popup.addAction(QTStr("Filters"), main_window, SLOT(OpenSceneFilters()));

      popup.addSeparator();

      this->_per_scene_transition_menu.reset(
          CreatePerSceneTransitionMenu(main_window));
      popup.addMenu(this->_per_scene_transition_menu.get());

      /* ---------------------- */

      QAction *multiviewAction = popup.addAction(QTStr("ShowInMultiview"));

      OBSSourceAutoRelease source = this->_scene_tree_items.GetCurrentScene();
      OBSDataAutoRelease sceneSettings =
          obs_source_get_private_settings(source);

      obs_data_set_default_bool(sceneSettings, "show_in_multiview", true);
      bool show = obs_data_get_bool(sceneSettings, "show_in_multiview");

      multiviewAction->setCheckable(true);
      multiviewAction->setChecked(show);

      auto showInMultiview = [main_window](OBSData settings) {
        bool show = obs_data_get_bool(settings, "show_in_multiview");
        obs_data_set_bool(settings, "show_in_multiview", !show);
        // Workaround because OBSProjector::UpdateMultiviewProjectors() isn't
        // available to modules
        QMetaObject::invokeMethod(main_window, "ScenesReordered");
      };

      connect(multiviewAction, &QAction::triggered,
              std::bind(showInMultiview, sceneSettings.Get()));

      copyFilters->setEnabled(obs_source_filter_count(source) > 0);
    } else if (item->type() == StvItemModel::FOLDER) {
      popup.addSeparator();

      // Rename folder
      QAction *rename = popup.addAction(QTStr("Rename"));
      QObject::connect(rename, SIGNAL(triggered()), this->_stv_dock.stvTree,
                       SLOT(EditSelectedItem()));

      // Remove folder (and all contents)
      auto removeFolder = [this, item]() { this->RemoveFolder(item); };
      popup.addAction(QTStr("Remove"), removeFolder);
    }

    popup.addSeparator();

    // Enable/disable scene or folder icon
    const auto toggleName =
        item->type() == StvItemModel::SCENE
            ? obs_module_text("SceneTreeView.ToggleSceneIcons")
            : obs_module_text("SceneTreeView.ToggleFolderIcons");

    QAction *toggleIconAction = popup.addAction(toggleName);
    toggleIconAction->setCheckable(true);

    const auto configName = item->type() == StvItemModel::SCENE
                                ? "ShowSceneIcons"
                                : "ShowFolderIcons";
    const bool showIcon = config_get_bool(obs_frontend_get_user_config(),
                                          "SceneTreeView", configName);

    toggleIconAction->setChecked(showIcon);

    auto toggleIcon = [this, showIcon, configName, item]() {
      config_set_bool(obs_frontend_get_user_config(), "SceneTreeView",
                      configName, !showIcon);
      this->_scene_tree_items.SetIconVisibility(
          !showIcon, (StvItemModel::QITEM_TYPE)item->type());
    };

    connect(toggleIconAction, &QAction::triggered, toggleIcon);
  }

  //	popup.addSeparator();

  //	bool grid = ui->scenes->GetGridMode();

  //	QAction *gridAction = new QAction(grid ? QTStr("Basic.Main.ListMode")
  //	                       : QTStr("Basic.Main.GridMode"),
  //	                  this);
  //	connect(gridAction, SIGNAL(triggered()), this,
  //	    SLOT(GridActionClicked()));
  //	popup.addAction(gridAction);

  popup.exec(QCursor::pos());
}

void ObsSceneTreeView::SelectCurrentScene() {
  QStandardItem* item = scene_tree_items_.GetCurrentSceneItem();
  if (item && item->index() != stv_dock_.stvTree->currentIndex()) {
    QMetaObject::invokeMethod(stv_dock_.stvTree, "setCurrentIndex",
                              Q_ARG(QModelIndex, item->index()));
  }
}

void ObsSceneTreeView::RemoveFolder(QStandardItem* folder) {
  int row = 0;
  int row_count = folder->rowCount();
  while (row < row_count) {
    QStandardItem* item = folder->child(row);
    assert(item->type() == StvItemModel::kFolder ||
           item->type() == StvItemModel::kScene);

    if (item->type() == StvItemModel::kScene) {
      obs_weak_source_t* weak =
          item->data(StvItemModel::kObsScene).value<ObsWeakSourcePtr>().ptr;
      OBSSourceAutoRelease source = OBSGetStrongRef(weak);

      scene_tree_items_.SetSelectedScene(
          item, obs_frontend_preview_program_mode_active());
      QMetaObject::invokeMethod(remove_scene_act_, "triggered");
    } else {
      RemoveFolder(item);
    }

    // Check if item was deleted. If not, move to next row.
    if (row_count == folder->rowCount()) {
      ++row;
    }

    row_count = folder->rowCount();
  }

  // Remove folder if empty.
  if (folder->rowCount() == 0) {
    scene_tree_items_.GetParentOrRoot(folder->index())
        ->removeRow(folder->row());
  }
}

}  // namespace scene_tree_view

Q_DECLARE_METATYPE(OBSSource);

namespace scene_tree_view {

static inline OBSSource GetTransitionComboItem(QComboBox* combo, int idx) {
  return combo->itemData(idx).value<OBSSource>();
}

QMenu* ObsSceneTreeView::CreatePerSceneTransitionMenu(QMainWindow* main_window) {
  OBSSourceAutoRelease scene = scene_tree_items_.GetCurrentScene();
  QMenu* menu = new QMenu(QTStr("TransitionOverride"));
  QAction* action;

  OBSDataAutoRelease sceneSettings = obs_source_get_private_settings(scene);

  obs_data_set_default_int(sceneSettings, "transition_duration", 300);

  const char* curTransition = obs_data_get_string(sceneSettings, "transition");
  int curDuration = (int)obs_data_get_int(sceneSettings, "transition_duration");

  QSpinBox* duration = new QSpinBox(menu);
  duration->setMinimum(50);
  duration->setSuffix(" ms");
  duration->setMaximum(20000);
  duration->setSingleStep(50);
  duration->setValue(curDuration);

  QComboBox* combo = main_window->findChild<QComboBox*>("transitions");
  assert(combo);

  auto setTransition = [this, combo](QAction* action) {
    int idx = action->property("transition_index").toInt();
    OBSSourceAutoRelease scene = scene_tree_items_.GetCurrentScene();
    OBSDataAutoRelease sceneSettings = obs_source_get_private_settings(scene);

    if (idx == -1) {
      obs_data_set_string(sceneSettings, "transition", "");
      return;
    }

    OBSSource tr = GetTransitionComboItem(combo, idx);

    if (tr) {
      const char* name = obs_source_get_name(tr);
      obs_data_set_string(sceneSettings, "transition", name);
    }
  };

  auto setDuration = [this](int duration) {
    OBSSourceAutoRelease scene = scene_tree_items_.GetCurrentScene();
    OBSDataAutoRelease sceneSettings = obs_source_get_private_settings(scene);

    obs_data_set_int(sceneSettings, "transition_duration", duration);
  };

  connect(duration, (void (QSpinBox::*)(int))&QSpinBox::valueChanged,
          setDuration);

  std::string none = "None";
  for (int i = -1; i < combo->count(); i++) {
    const char* name = "";

    if (i >= 0) {
      OBSSource tr;
      tr = GetTransitionComboItem(combo, i);
      if (!tr) {
        continue;
      }
      name = obs_source_get_name(tr);
    }

    bool match = (name && strcmp(name, curTransition) == 0);

    if (!name || !*name) {
      name = none.c_str();
    }

    action = menu->addAction(QT_UTF8(name));
    action->setProperty("transition_index", i);
    action->setCheckable(true);
    action->setChecked(match);

    connect(action, &QAction::triggered, std::bind(setTransition, action));
  }

  QWidgetAction* durationAction = new QWidgetAction(menu);
  durationAction->setDefaultWidget(duration);

  menu->addSeparator();
  menu->addAction(durationAction);
  return menu;
}

void ObsSceneTreeView::ObsFrontendEvent(enum obs_frontend_event event) {
  if (event == OBS_FRONTEND_EVENT_FINISHED_LOADING) {
    if (!g_stv_added && g_stv_dock) {
      const char* t = obs_module_text("SceneTreeView.Title");
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

    auto applyThemeAndIcons = [this]() {
      QMainWindow* main_window =
          reinterpret_cast<QMainWindow*>(obs_frontend_get_main_window());
      if (add_scene_act_) {
        stv_dock_.stvAdd->setIcon(add_scene_act_->icon());
      }
      if (remove_scene_act_) {
        stv_dock_.stvRemove->setIcon(remove_scene_act_->icon());
      }
      stv_dock_.stvAddFolder->setIcon(
          main_window->property("groupIcon").value<QIcon>());

      if (move_scene_up_act_) {
        stv_dock_.stvMoveUp->setIcon(
            NonDimmedDisabled(move_scene_up_act_->icon()));
      }
      if (move_scene_down_act_) {
        stv_dock_.stvMoveDown->setIcon(
            NonDimmedDisabled(move_scene_down_act_->icon()));
      }

      auto copyProps = [](QAction* act, QWidget* w) {
        if (!act || !w) {
          return;
        }
        const auto names = act->dynamicPropertyNames();
        for (const QByteArray& n : names) {
          w->setProperty(n.constData(), act->property(n.constData()));
        }
      };
      copyProps(add_scene_act_, stv_dock_.stvAdd);
      copyProps(remove_scene_act_, stv_dock_.stvRemove);

      QString addCls = add_scene_act_
                           ? add_scene_act_->property("class").toString()
                           : QString();

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
    };

    applyThemeAndIcons();

    stv_dock_.stvAdd->setEnabled(true);
    stv_dock_.stvRemove->setEnabled(true);
    stv_dock_.stvAddFolder->setEnabled(true);

    stv_dock_.stvMoveUp->setEnabled(true);
    stv_dock_.stvMoveDown->setEnabled(true);
    UpdateMoveButtonsEnabled();

    static bool tools_menu_added = false;
    if (!tools_menu_added) {
      tools_menu_added = true;
      QAction* about_action = static_cast<QAction*>(
          obs_frontend_add_tools_menu_qaction(obs_module_name()));
      if (about_action) {
        connect(about_action, &QAction::triggered, this, [this]() {
          QMainWindow* main_window =
              reinterpret_cast<QMainWindow*>(obs_frontend_get_main_window());
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

          QPushButton* history_btn = mb.addButton(
              obs_module_text("SceneTreeView.VersionHistory"), QMessageBox::ActionRole);
          mb.addButton(QMessageBox::Ok);
          mb.exec();

          if (mb.clickedButton() == history_btn) {
            QDialog history_dialog(main_window);
            history_dialog.setWindowTitle(
                QString::fromUtf8(obs_module_name()) + QStringLiteral(" - ") +
                QString::fromUtf8(obs_module_text("SceneTreeView.VersionHistory")));
            QVBoxLayout* layout = new QVBoxLayout(&history_dialog);
            QTextBrowser* browser = new QTextBrowser(&history_dialog);
            browser->setOpenExternalLinks(true);
            browser->setHtml(QStringLiteral(
                "<h3>Version History</h3>"
                "<hr/>"
                "<b>v0.2.2</b> (Anthony Mendez)"
                "<ul>"
                "<li><b>Google C++ Style Guide Alignment:</b> Refactored classes, headers, namespaces, and variables to conform to Google C++ Style guidelines.</li>"
                "<li><b>Improved About Dialog:</b> Integrated a dedicated, scrollable version history window.</li>"
                "<li><b>Author Identity Alignment:</b> Standardized local repository commits and contributors.</li>"
                "</ul>"
                "<b>v0.2.1</b> (Anthony Mendez)"
                "<ul>"
                "<li><b>Automated Linux Scripting:</b> Introduced automated build and install script for Arch/CachyOS.</li>"
                "<li><b>NTFS Compilation Fix:</b> Fixed GNU BFD linker crashes on NTFS partitions via ld.lld fallback.</li>"
                "<li><b>Tools Menu Integration:</b> Added a Tools menu action to display QMessageBox.</li>"
                "<li><b>Metadata Centralization:</b> Centralized project metadata in buildspec.json.</li>"
                "</ul>"
                "<b>v0.2.0</b> (Anthony Mendez, TheThirdRail, John Titor, DigitOtter, Marcelo dos Santos Mafra)"
                "<ul>"
                "<li><b>Cross-Platform Support:</b> Enabled Linux and macOS compilation using CMake.</li>"
                "<li><b>macOS Universal Binaries:</b> Added multi-architecture support and Qt 6 framework packaging.</li>"
                "<li><b>CI Workflows:</b> Configured GitHub Actions release workflows.</li>"
                "</ul>"
                "<b>v0.1.9 / v0.1.12</b> (DigitOtter)"
                "<ul>"
                "<li><b>macOS Qt 6.8:</b> Added compatibility fixes for dependencies.</li>"
                "</ul>"
                "<b>v0.1.8</b> (DigitOtter)"
                "<ul>"
                "<li><b>Button-driven Reordering:</b> Added Move Up/Down buttons to the dock toolbar.</li>"
                "<li><b>Theme-aware Icons:</b> Wired dynamic icon styles for toolbar buttons.</li>"
                "<li><b>Folder Creation Heuristics:</b> Added duplicate folder check to generate unique names.</li>"
                "</ul>"
                "<b>v0.1.7</b> (DigitOtter, Marcelo dos Santos Mafra, Borlader)"
                "<ul>"
                "<li><b>OBS Studio 32+ Support:</b> Fixed scene renaming hooks for OBS 28/29.</li>"
                "<li><b>Russian Localization:</b> Added translation files.</li>"
                "<li><b>Global Duplicate Check:</b> Enhanced uniqueness scan across the entire tree.</li>"
                "<li><b>Initial Dock Behavior:</b> Set dock to start hidden on first load.</li>"
                "<li><b>Author Identity Alignment:</b> Standardized local repository commits and contributors.</li>"
                "</ul>"
                "<b>v0.1.6</b> (DigitOtter)"
                "<ul>"
                "<li><b>Content Sizing:</b> Fixed tree view layout resizing bugs.</li>"
                "<li><b>Checksum Utility:</b> Automated zip verification.</li>"
                "</ul>"
                "<b>v0.1.5</b> (DigitOtter)"
                "<ul>"
                "<li><b>Windows Build Guide:</b> Documented build guidelines for Windows.</li>"
                "<li><b>Button Class Sync:</b> Matched dynamic properties to OBS styles.</li>"
                "</ul>"
                "<b>v0.1.0 (Initial Release)</b> (DigitOtter, Marcelo dos Santos Mafra)"
                "<ul>"
                "<li><b>Hierarchical Dock:</b> Created custom tree structure layout in OBS Studio.</li>"
                "<li><b>Nesting & Drag-and-Drop:</b> Supported folders and direct index sorting.</li>"
                "<li><b>Locale Framework:</b> Added pt-BR and en-US locales.</li>"
                "</ul>"
            ));
            layout->addWidget(browser);
            history_dialog.resize(550, 450);
            history_dialog.exec();
          }
        });
      }
    }

  } else if (event == OBS_FRONTEND_EVENT_THEME_CHANGED) {
    QMainWindow* main_window =
        reinterpret_cast<QMainWindow*>(obs_frontend_get_main_window());
    if (add_scene_act_) {
      stv_dock_.stvAdd->setIcon(add_scene_act_->icon());
    }
    if (remove_scene_act_) {
      stv_dock_.stvRemove->setIcon(remove_scene_act_->icon());
    }
    stv_dock_.stvAddFolder->setIcon(
        main_window->property("groupIcon").value<QIcon>());

    if (move_scene_up_act_) {
      stv_dock_.stvMoveUp->setIcon(
          NonDimmedDisabled(move_scene_up_act_->icon()));
    }
    if (move_scene_down_act_) {
      stv_dock_.stvMoveDown->setIcon(
          NonDimmedDisabled(move_scene_down_act_->icon()));
    }

    auto copyProps = [](QAction* act, QWidget* w) {
      if (!act || !w) {
        return;
      }

      const auto names = act->dynamicPropertyNames();
      for (const QByteArray& n : names) {
        w->setProperty(n.constData(), act->property(n.constData()));
      }
    };
    copyProps(add_scene_act_, stv_dock_.stvAdd);
    copyProps(remove_scene_act_, stv_dock_.stvRemove);

    copyProps(move_scene_up_act_, stv_dock_.stvMoveUp);
    copyProps(move_scene_down_act_, stv_dock_.stvMoveDown);

    QString addCls = add_scene_act_
                         ? add_scene_act_->property("class").toString()
                         : QString();
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

    QString upCls = move_scene_up_act_
                        ? move_scene_up_act_->property("class").toString()
                        : QString();
    QString downCls = move_scene_down_act_
                          ? move_scene_down_act_->property("class").toString()
                          : QString();

    if (!remCls.isEmpty()) {
      removeClasses += QStringLiteral(" ") + remCls;
    } else {
      removeClasses += QStringLiteral(" icon-trash");
    }
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

  } else if (event == OBS_FRONTEND_EVENT_SCENE_LIST_CHANGED) {
    UpdateTreeView();
  } else if (event == OBS_FRONTEND_EVENT_SCENE_CHANGED ||
             event == OBS_FRONTEND_EVENT_PREVIEW_SCENE_CHANGED) {
    SelectCurrentScene();
  } else if (event == OBS_FRONTEND_EVENT_SCENE_COLLECTION_CLEANUP) {
    scene_tree_items_.CleanupSceneTree();
    scene_collection_name_ = nullptr;
  } else if (event == OBS_FRONTEND_EVENT_SCENE_COLLECTION_CHANGING) {
    SaveSceneTree(scene_collection_name_);
  } else if (event == OBS_FRONTEND_EVENT_SCENE_COLLECTION_CHANGED) {
    scene_collection_name_ = obs_frontend_get_current_scene_collection();
    LoadSceneTree(scene_collection_name_);
    UpdateTreeView();
  } else if (event == OBS_FRONTEND_EVENT_SCENE_COLLECTION_RENAMED) {
    scene_collection_name_ = obs_frontend_get_current_scene_collection();
    SaveSceneTree(scene_collection_name_);
    UpdateTreeView();
  }
}

void ObsSceneTreeView::ObsFrontendSave(obs_data_t* /*save_data*/, bool saving) {
  if (saving) {
    SaveSceneTree(scene_collection_name_);
  }
}

void ObsSceneTreeView::on_stvMoveUp_released() {
  const QModelIndex idx = stv_dock_.stvTree->currentIndex();
  if (!idx.isValid()) {
    return;
  }
  const int oldRow = idx.row();
  QStandardItem* parent = scene_tree_items_.GetParentOrRoot(idx);
  if (scene_tree_items_.MoveIndexByOne(idx, -1)) {
    SaveSceneTree(scene_collection_name_);
    if (parent && oldRow - 1 >= 0 && oldRow - 1 < parent->rowCount()) {
      stv_dock_.stvTree->setCurrentIndex(parent->child(oldRow - 1)->index());
    }
  }
  UpdateMoveButtonsEnabled();
}

void ObsSceneTreeView::on_stvMoveDown_released() {
  const QModelIndex idx = stv_dock_.stvTree->currentIndex();
  if (!idx.isValid()) {
    return;
  }
  const int oldRow = idx.row();
  QStandardItem* parent = scene_tree_items_.GetParentOrRoot(idx);
  if (scene_tree_items_.MoveIndexByOne(idx, +1)) {
    SaveSceneTree(scene_collection_name_);
    if (parent && oldRow + 1 < parent->rowCount()) {
      stv_dock_.stvTree->setCurrentIndex(parent->child(oldRow + 1)->index());
    }
  }
  UpdateMoveButtonsEnabled();
}

void ObsSceneTreeView::UpdateMoveButtonsEnabled() {
  bool enableUp = false;
  bool enableDown = false;
  const QModelIndex idx = stv_dock_.stvTree->currentIndex();
  if (idx.isValid()) {
    QStandardItem* parent = scene_tree_items_.GetParentOrRoot(idx);
    const int row = idx.row();
    const int count = parent ? parent->rowCount() : 0;
    enableUp = (row > 0);
    enableDown = (row >= 0 && row < count - 1);
  }
  stv_dock_.stvMoveUp->setEnabled(enableUp);
  stv_dock_.stvMoveDown->setEnabled(enableDown);
}

}  // namespace scene_tree_view
