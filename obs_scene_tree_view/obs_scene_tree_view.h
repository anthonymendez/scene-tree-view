// Copyright (c) 2026 Anthony Mendez. All rights reserved.
// Use of this source code is governed by the GPL-2.0 license that can be
// found in the LICENSE file.

#ifndef OBS_SCENE_TREE_VIEW_OBS_SCENE_TREE_VIEW_H_
#define OBS_SCENE_TREE_VIEW_OBS_SCENE_TREE_VIEW_H_

#include <map>
#include <memory>
#include <string_view>

#include <QAbstractItemDelegate>
#include <QtWidgets/QLabel>
#include <QtWidgets/QWidget>
#include <QTimer>

#include <obs-data.h>
#include <util/util.hpp>

#include "obs_scene_tree_view/stv_item_model.h"
#include "ui_scene_tree_view.h"

namespace scene_tree_view {

// Custom dock widget providing a tree-structured scene collection for OBS.
// Displays scenes, custom folders, allows reordering/nesting, and maps selection
// and context menu actions directly into active OBS frontend contexts.
class ObsSceneTreeView : public QWidget {
  Q_OBJECT

 public:
  // Default configuration filename for storing scene tree state.
  static constexpr std::string_view kSceneTreeConfigFile = "scene_tree.json";

  // Creates the scene tree dock widget and initializes its child UI layouts.
  explicit ObsSceneTreeView(QMainWindow* main_window);
  ~ObsSceneTreeView() override;

  // Saves the scene tree layout for the specified scene collection.
  void SaveSceneTree(const char* scene_collection);

  // Loads the scene tree layout for the specified scene collection.
  void LoadSceneTree(const char* scene_collection);

 protected slots:
  // Re-synchronizes the scene tree item model with the active OBS scene list.
  void UpdateTreeView();

  // Toggles the visibility of toolbar items in the dock window.
  void on_toggleListboxToolbars(bool visible);

  // Creates and adds a new folder item at the current selection level.
  void on_stvAddFolder_clicked();

  // Initiates deletion logic for the currently selected folder or scene.
  void on_stvRemove_released();

  // Shifts the selected item up by one index within its current parent level.
  void on_stvMoveUp_released();

  // Shifts the selected item down by one index within its current parent level.
  void on_stvMoveDown_released();

  // Updates the enablement status of the move up/down tool buttons.
  void UpdateMoveButtonsEnabled();

  // Spawns a custom context menu for the scene tree item at the specified position.
  void on_stvTree_customContextMenuRequested(const QPoint& pos);

  // Finalizes renaming operations on folder items.
  void on_SceneNameEdited(QWidget* editor);

 private:
  // Actions linked from the main OBS window context.
  QAction* add_scene_act_ = nullptr;
  QAction* remove_scene_act_ = nullptr;
  QAction* toggle_toolbars_scene_act_ = nullptr;
  QAction* move_scene_up_act_ = nullptr;
  QAction* move_scene_down_act_ = nullptr;

  // Custom submenu override to handle per-scene transition mappings.
  std::unique_ptr<QMenu> per_scene_transition_menu_;

  // Auto-generated UI form class mapping layout components.
  Ui::STVDock stv_dock_;

  // Underlying data model representing current folder/scene hierarchy.
  StvItemModel scene_tree_items_;

  // Cached name of the active OBS scene collection.
  BPtr<char> scene_collection_name_ = nullptr;

  // Loading screen components
  QLabel* loading_label_ = nullptr;
  QTimer* loading_timer_ = nullptr;
  QTimer* loading_timeout_ = nullptr;
  int loading_dots_count_ = 0;

  void ShowLoadingScreen();
  void HideLoadingScreen();
  bool HasUnassociatedScenes() const;
  bool HasUnassociatedScenesHelper(QStandardItem* parent) const;

  // Selects and focuses the active OBS scene in the tree widget.
  void SelectCurrentScene();

  // Recursively removes folder items and deletes any nested OBS scenes.
  void RemoveFolder(QStandardItem* folder);

  // Generates transition override context menus for individual scenes.
  QMenu* CreatePerSceneTransitionMenu(QMainWindow* main_window);

  // Static event callback forwarder registered into the OBS frontend system.
  inline static void obs_frontend_event_cb(enum obs_frontend_event event,
                                           void* private_data) {
    reinterpret_cast<ObsSceneTreeView*>(private_data)->ObsFrontendEvent(event);
  }

  // Static save/load callback forwarder registered into the OBS frontend system.
  inline static void obs_frontend_save_cb(obs_data_t* save_data, bool saving,
                                          void* private_data) {
    reinterpret_cast<ObsSceneTreeView*>(private_data)->ObsFrontendSave(
        save_data, saving);
  }

  // Internal handler responding to OBS lifecycle and scene changes.
  void ObsFrontendEvent(enum obs_frontend_event event);

  // Internal handler saving configuration variables when OBS initiates updates.
  void ObsFrontendSave(obs_data_t* save_data, bool saving);
};

}  // namespace scene_tree_view

#endif  // OBS_SCENE_TREE_VIEW_OBS_SCENE_TREE_VIEW_H_
