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

#include <functional>
#include <obs-frontend-api.h>
#include <obs-module.h>
#include <util/platform.h>

#define QT_UTF8(str) QString::fromUtf8(str)
#define QT_TO_UTF8(str) str.toUtf8().constData()

Q_DECLARE_METATYPE(OBSSource);

namespace scene_tree_view {

void ObsSceneTreeView::on_toggleListboxToolbars(bool visible) {
  stv_dock_.listbox->setVisible(visible);
}

void ObsSceneTreeView::on_stvAddFolder_clicked() {
  int row;
  QStandardItem *selected =
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
  while (
      !scene_tree_items_.CheckFolderNameUniqueness(new_folder_name, selected)) {
    new_folder_name = format.arg(++i);
  }

  StvFolderItem *pItem = new StvFolderItem(new_folder_name);
  selected->insertRow(row, pItem);
  scene_tree_items_.AddChildToUserOrder(selected, pItem);
  scene_tree_items_.SortFolder(selected);

  SaveSceneTree(scene_collection_name_);
}

static void CollectScenes(QStandardItem *item, QList<QStandardItem *> &scenes) {
  if (!item) {
    return;
  }
  if (item->type() == StvItemModel::kScene) {
    if (!scenes.contains(item)) {
      scenes.append(item);
    }
  } else if (item->type() == StvItemModel::kFolder) {
    for (int i = 0; i < item->rowCount(); ++i) {
      CollectScenes(item->child(i), scenes);
    }
  }
}

void ObsSceneTreeView::on_stvRemove_released() {
  QItemSelectionModel *selection_model = stv_dock_.stvTree->selectionModel();
  if (!selection_model) {
    return;
  }

  QModelIndexList selected_indexes = selection_model->selectedIndexes();
  QList<QStandardItem *> selected_items;
  for (const QModelIndex &index : selected_indexes) {
    if (index.column() != 0) {
      continue;
    }
    QStandardItem *item = scene_tree_items_.itemFromIndex(index);
    if (item && !selected_items.contains(item)) {
      selected_items.append(item);
    }
  }

  if (selected_items.isEmpty()) {
    QModelIndex curr = stv_dock_.stvTree->currentIndex();
    if (curr.isValid()) {
      QStandardItem *item = scene_tree_items_.itemFromIndex(curr);
      if (item) {
        selected_items.append(item);
      }
    }
  }

  if (selected_items.isEmpty()) {
    return;
  }

  // Filter to keep only top-level selected items
  QList<QStandardItem *> top_level_selected;
  for (QStandardItem *item : selected_items) {
    bool ancestor_selected = false;
    QStandardItem *parent = item->parent();
    while (parent) {
      if (selected_items.contains(parent)) {
        ancestor_selected = true;
        break;
      }
      parent = parent->parent();
    }
    if (!ancestor_selected) {
      top_level_selected.append(item);
    }
  }

  // Collect all scenes recursively
  QList<QStandardItem *> scenes_to_delete;
  for (QStandardItem *item : top_level_selected) {
    CollectScenes(item, scenes_to_delete);
  }

  if (scenes_to_delete.size() > 1) {
    // Multi-delete confirmation
    QMainWindow *main_window =
        reinterpret_cast<QMainWindow *>(obs_frontend_get_main_window());

    QString title =
        QString::fromUtf8(obs_module_text("SceneTreeView.MultiDelete.Confirm"));
    if (title.isEmpty() ||
        title == QLatin1String("SceneTreeView.MultiDelete.Confirm")) {
      title = QStringLiteral("Delete Multiple Scenes");
    }

    QString text =
        QString::fromUtf8(obs_module_text("SceneTreeView.MultiDelete.Text"));
    if (text.isEmpty() ||
        text == QLatin1String("SceneTreeView.MultiDelete.Text")) {
      text = QStringLiteral(
          "Are you sure you want to delete the following scenes?");
    }

    QString list_html = QStringLiteral("<ul>");
    for (QStandardItem *item : scenes_to_delete) {
      list_html +=
          QStringLiteral("<li>%1</li>").arg(item->text().toHtmlEscaped());
    }
    list_html += QStringLiteral("</ul>");

    QMessageBox mb(main_window);
    mb.setWindowTitle(title);
    mb.setIcon(QMessageBox::Question);
    mb.setTextFormat(Qt::RichText);
    mb.setText(QStringLiteral("<p>%1</p>%2").arg(text, list_html));
    mb.setStandardButtons(QMessageBox::Yes | QMessageBox::No);
    mb.setDefaultButton(QMessageBox::No);

    if (mb.exec() == QMessageBox::Yes) {
      // First, get all the raw source pointers and add a reference to them
      QList<obs_source_t *> raw_sources;
      for (QStandardItem *item : scenes_to_delete) {
        obs_weak_source_t *weak =
            item->data(StvItemModel::kObsScene).value<ObsWeakSourcePtr>().ptr;
        OBSSourceAutoRelease source = OBSGetStrongRef(weak);
        if (source) {
          obs_source_t *ref = obs_source_get_ref(source.Get());
          if (ref) {
            raw_sources.append(ref);
          }
        }
      }

      // Gather persistent model indexes of the top-level folders to remove
      QList<QPersistentModelIndex> folders_to_remove;
      for (QStandardItem *item : top_level_selected) {
        if (item->type() == StvItemModel::kFolder) {
          folders_to_remove.append(QPersistentModelIndex(item->index()));
        }
      }

      // Remove scenes from OBS
      for (obs_source_t *source : raw_sources) {
        obs_source_remove(source);
        obs_source_release(source); // Release our reference
      }

      // Remove top-level folders from model using their persistent indexes
      for (const QPersistentModelIndex &p_idx : folders_to_remove) {
        if (p_idx.isValid()) {
          QStandardItem *folder_item = scene_tree_items_.itemFromIndex(p_idx);
          if (folder_item) {
            QStandardItem *parent =
                scene_tree_items_.GetParentOrRoot(folder_item->index());
            if (parent) {
              parent->removeRow(folder_item->row());
            }
          }
        }
      }

      UpdateTreeView();
    }
  } else {
    // Fallback to original delete logic for 0 or 1 scenes
    for (QStandardItem *selected : top_level_selected) {
      if (selected->type() == StvItemModel::kScene) {
        scene_tree_items_.SetSelectedScene(
            selected, obs_frontend_preview_program_mode_active());
        QMetaObject::invokeMethod(remove_scene_act_, "triggered");
      } else {
        RemoveFolder(selected);
      }
    }
  }
}

void ObsSceneTreeView::on_stvTree_customContextMenuRequested(
    const QPoint &pos) {
  QStandardItem *item =
      scene_tree_items_.itemFromIndex(stv_dock_.stvTree->indexAt(pos));

  QMainWindow *main_window =
      reinterpret_cast<QMainWindow *>(obs_frontend_get_main_window());

  QMenu popup(this);

  popup.addAction(obs_module_text("SceneTreeView.AddScene"), main_window,
                  SLOT(on_actionAddScene_triggered()));

  popup.addAction(obs_module_text("SceneTreeView.AddFolder"), this,
                  SLOT(on_stvAddFolder_clicked()));

  popup.addSeparator();

  QStandardItem *folder_item = nullptr;
  if (!item) {
    folder_item = scene_tree_items_.invisibleRootItem();
  } else if (item->type() == StvItemModel::kFolder) {
    folder_item = item;
  } else {
    folder_item = scene_tree_items_.GetParentOrRoot(item->index());
  }

  QActionGroup *sortGroup = new QActionGroup(&popup);

  QAction *sortUser =
      popup.addAction(obs_module_text("SceneTreeView.SortByUser"));
  sortUser->setCheckable(true);
  sortGroup->addAction(sortUser);

  QAction *sortAsc = popup.addAction(obs_module_text("SceneTreeView.SortAsc"));
  sortAsc->setCheckable(true);
  sortGroup->addAction(sortAsc);

  QAction *sortDesc =
      popup.addAction(obs_module_text("SceneTreeView.SortDesc"));
  sortDesc->setCheckable(true);
  sortGroup->addAction(sortDesc);

  int current_mode = folder_item->data(StvItemModel::kSortMode).toInt();
  if (current_mode == static_cast<int>(StvItemModel::SortMode::kAlphaAsc)) {
    sortAsc->setChecked(true);
  } else if (current_mode ==
             static_cast<int>(StvItemModel::SortMode::kAlphaDesc)) {
    sortDesc->setChecked(true);
  } else {
    sortUser->setChecked(true);
  }

  auto setSortMode = [this, folder_item](StvItemModel::SortMode mode) {
    int current_mode = folder_item->data(StvItemModel::kSortMode).toInt();
    if (current_mode == static_cast<int>(StvItemModel::SortMode::kUser) &&
        mode != StvItemModel::SortMode::kUser) {
      QVariantList user_list;
      for (int j = 0; j < folder_item->rowCount(); ++j) {
        QStandardItem *child = folder_item->child(j);
        QVariantMap map;
        map["name"] = child->text();
        map["type"] = child->type();
        user_list.append(map);
      }
      folder_item->setData(user_list, StvItemModel::kUserOrder);
    }

    folder_item->setData(static_cast<int>(mode), StvItemModel::kSortMode);

    if (mode == StvItemModel::SortMode::kUser) {
      scene_tree_items_.RestoreUserOrder(folder_item);
      folder_item->setData(QVariant(), StvItemModel::kUserOrder);
    } else {
      scene_tree_items_.SortFolder(folder_item);
    }

    SaveSceneTree(scene_collection_name_);
  };

  connect(sortUser, &QAction::triggered,
          [setSortMode]() { setSortMode(StvItemModel::SortMode::kUser); });
  connect(sortAsc, &QAction::triggered,
          [setSortMode]() { setSortMode(StvItemModel::SortMode::kAlphaAsc); });
  connect(sortDesc, &QAction::triggered,
          [setSortMode]() { setSortMode(StvItemModel::SortMode::kAlphaDesc); });

  if (item) {
    if (item->type() == StvItemModel::kScene) {
      QAction *copyFilters = new QAction(QTStr("Copy.Filters"), this);
      copyFilters->setEnabled(false);
      connect(copyFilters, SIGNAL(triggered()), main_window,
              SLOT(SceneCopyFilters()));
      QAction *pasteFilters = new QAction(QTStr("Paste.Filters"), this);
      connect(pasteFilters, SIGNAL(triggered()), main_window,
              SLOT(ScenePasteFilters()));

      popup.addSeparator();
      popup.addAction(QTStr("Duplicate"), main_window,
                      SLOT(DuplicateSelectedScene()));
      popup.addAction(copyFilters);
      popup.addAction(pasteFilters);
      popup.addSeparator();
      QAction *rename = popup.addAction(QTStr("Rename"));
      QObject::connect(rename, SIGNAL(triggered()), stv_dock_.stvTree,
                       SLOT(EditSelectedItem()));
      popup.addAction(QTStr("Remove"), this, SLOT(on_stvRemove_released()));
      popup.addSeparator();

      QAction *sceneWindow = popup.addAction(QTStr("SceneWindow"), main_window,
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

      QAction *multiviewAction = popup.addAction(QTStr("ShowInMultiview"));

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

    QAction *toggleIconAction = popup.addAction(toggleName);
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
      config_save(obs_frontend_get_user_config());
      scene_tree_items_.SetIconVisibility(
          !showIcon, (StvItemModel::QItemType)item->type());
    };

    connect(toggleIconAction, &QAction::triggered, toggleIcon);
  }

  popup.exec(QCursor::pos());
}

void ObsSceneTreeView::on_SceneNameEdited(QWidget *editor) {
  QStandardItem *selected =
      scene_tree_items_.itemFromIndex(stv_dock_.stvTree->currentIndex());
  if (selected->type() == StvItemModel::kScene) {
    QMainWindow *main_window =
        reinterpret_cast<QMainWindow *>(obs_frontend_get_main_window());
    QMetaObject::invokeMethod(main_window, "SceneNameEdited",
                              Q_ARG(QWidget *, editor));
  } else {
    QLineEdit *edit = qobject_cast<QLineEdit *>(editor);
    std::string text = QT_TO_UTF8(edit->text().trimmed());
    (void)text; // Suppress unused warning

    selected->setText(scene_tree_items_.CreateUniqueFolderName(
        selected, scene_tree_items_.GetParentOrRoot(selected->index())));

    QStandardItem *parent =
        scene_tree_items_.GetParentOrRoot(selected->index());
    scene_tree_items_.SortFolder(parent);
    SaveSceneTree(scene_collection_name_);
  }
}

void ObsSceneTreeView::RemoveFolder(QStandardItem *folder) {
  int row = 0;
  int row_count = folder->rowCount();
  while (row < row_count) {
    QStandardItem *item = folder->child(row);
    assert(item->type() == StvItemModel::kFolder ||
           item->type() == StvItemModel::kScene);

    if (item->type() == StvItemModel::kScene) {
      obs_weak_source_t *weak =
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

void ObsSceneTreeView::on_stvMoveUp_released() {
  const QModelIndex idx = stv_dock_.stvTree->currentIndex();
  if (!idx.isValid()) {
    return;
  }
  const int oldRow = idx.row();
  QStandardItem *parent = scene_tree_items_.GetParentOrRoot(idx);
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
  QStandardItem *parent = scene_tree_items_.GetParentOrRoot(idx);
  if (scene_tree_items_.MoveIndexByOne(idx, +1)) {
    SaveSceneTree(scene_collection_name_);
    if (parent && oldRow + 1 < parent->rowCount()) {
      stv_dock_.stvTree->setCurrentIndex(parent->child(oldRow + 1)->index());
    }
  }
  UpdateMoveButtonsEnabled();
}

static inline OBSSource GetTransitionComboItem(QComboBox *combo, int idx) {
  return combo->itemData(idx).value<OBSSource>();
}

QMenu *
ObsSceneTreeView::CreatePerSceneTransitionMenu(QMainWindow *main_window) {
  OBSSourceAutoRelease scene = scene_tree_items_.GetCurrentScene();
  QMenu *menu = new QMenu(QTStr("TransitionOverride"));
  QAction *action;

  OBSDataAutoRelease sceneSettings = obs_source_get_private_settings(scene);

  obs_data_set_default_int(sceneSettings, "transition_duration", 300);

  const char *curTransition = obs_data_get_string(sceneSettings, "transition");
  int curDuration = (int)obs_data_get_int(sceneSettings, "transition_duration");

  QSpinBox *duration = new QSpinBox(menu);
  duration->setMinimum(50);
  duration->setSuffix(" ms");
  duration->setMaximum(20000);
  duration->setSingleStep(50);
  duration->setValue(curDuration);

  QComboBox *combo = main_window->findChild<QComboBox *>("transitions");
  assert(combo);

  auto setTransition = [this, combo](QAction *action) {
    int idx = action->property("transition_index").toInt();
    OBSSourceAutoRelease scene = scene_tree_items_.GetCurrentScene();
    OBSDataAutoRelease sceneSettings = obs_source_get_private_settings(scene);

    if (idx == -1) {
      obs_data_set_string(sceneSettings, "transition", "");
      return;
    }

    OBSSource tr = GetTransitionComboItem(combo, idx);

    if (tr) {
      const char *name = obs_source_get_name(tr);
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
    const char *name = "";

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

  QWidgetAction *durationAction = new QWidgetAction(menu);
  durationAction->setDefaultWidget(duration);

  menu->addSeparator();
  menu->addAction(durationAction);
  return menu;
}

} // namespace scene_tree_view
