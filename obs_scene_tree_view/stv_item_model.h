// Copyright (c) 2026 Anthony Mendez. All rights reserved.
// Use of this source code is governed by the GPL-2.0 license that can be
// found in the LICENSE file.

#ifndef OBS_SCENE_TREE_VIEW_STV_ITEM_MODEL_H_
#define OBS_SCENE_TREE_VIEW_STV_ITEM_MODEL_H_

#include <list>
#include <map>
#include <string_view>

#include <QStandardItemModel>
#include <QTreeView>
#include <QtWidgets/QMainWindow>

#include <obs-frontend-api.h>
#include <obs-module.h>
#include <obs.hpp>

namespace scene_tree_view {

// Wrapper struct for obs_weak_source_t to allow its usage in QVariant metatype.
struct ObsWeakSourcePtr {
  obs_weak_source_t* ptr;
};

}  // namespace scene_tree_view

Q_DECLARE_METATYPE(scene_tree_view::ObsWeakSourcePtr);

namespace scene_tree_view {

// Standard item representing a folder node in the scene tree view.
class StvFolderItem : public QStandardItem {
 public:
  // Creates a new folder item with the given display text.
  explicit StvFolderItem(const QString& text);
  ~StvFolderItem() override = default;

  // Returns the custom item type code.
  int type() const override;
};

// Standard item representing an OBS scene node in the scene tree view.
class StvSceneItem : public QStandardItem {
 public:
  // Creates a new scene item with the given display text and weak source reference.
  StvSceneItem(const QString& text, obs_weak_source_t* weak);
  ~StvSceneItem() override = default;

  // Returns the custom item type code.
  int type() const override;
};

// Item model representing the scene and folder hierarchy for the custom dock.
// Handles data serialization/deserialization to config files, drag-and-drop
// MIME type formatting, and syncing with the active OBS scene collections.
class StvItemModel : public QStandardItemModel {
  Q_OBJECT

  // Structure representing the base dimensions of the scenes.
  struct SceneSize {
    uint32_t cx;
    uint32_t cy;
  };

  // MIME type string for serializing drag-and-drop index data.
  static constexpr std::string_view kMimeType = "application/x-stvindexlist";

  // Configuration JSON keys.
  static constexpr std::string_view kSceneTreeConfigFolderData = "folder";
  static constexpr std::string_view kSceneTreeConfigFolderExpanded = "is_expanded";
  static constexpr std::string_view kSceneTreeConfigItemNameData = "name";

 public:
  // Custom sort modes for folder contents.
  enum class SortMode { kUser = 0, kAlphaAsc, kAlphaDesc };

  // Custom roles for standard item data lookup.
  enum QDataRole { kObsScene = Qt::UserRole, kSortMode, kUserOrder };

  // Custom standard item type identifiers.
  enum QItemType { kFolder = QStandardItem::UserType + 1, kScene };

  StvItemModel();
  ~StvItemModel() override;

  // Returns the MIME types supported by this model.
  QStringList mimeTypes() const override;

  // Serializes the dragged indexes into MIME data payload.
  QMimeData* mimeData(const QModelIndexList& indexes) const override;

  // Deserializes and handles dropping of MIME data payload at the given location.
  bool dropMimeData(const QMimeData* data, Qt::DropAction action, int row,
                    int column, const QModelIndex& parent) override;

  // Synchronizes the item model tree with the active OBS scene list.
  void UpdateTree(obs_frontend_source_list& scene_list,
                  const QModelIndex& selected_index);

  // Verifies that a folder name is unique within its parent hierarchy.
  bool CheckFolderNameUniqueness(const QString& name, QStandardItem* parent,
                                 QStandardItem* item_to_skip = nullptr);

  // Selects/activates the given scene in OBS Studio.
  void SetSelectedScene(QStandardItem* item, bool set_preview_scene,
                        bool force_set_scene = false);

  // Returns the standard item associated with the current active OBS scene.
  QStandardItem* GetCurrentSceneItem();

  // Returns a reference to the active OBS scene source.
  OBSSourceAutoRelease GetCurrentScene();

  // Serializes and saves the scene hierarchy into the JSON configuration profile.
  void SaveSceneTree(obs_data_t* root_folder_data, const char* scene_collection,
                     QTreeView* view);

  // Deserializes and loads the scene hierarchy from the JSON configuration profile.
  void LoadSceneTree(obs_data_t* root_folder_data, const char* scene_collection,
                     QTreeView* view);

  // Clears and releases all internal scene references and folder items.
  void CleanupSceneTree();

  // Returns the parent item or the invisible root item for the given index.
  QStandardItem* GetParentOrRoot(const QModelIndex& index);

  // Generates a unique folder name within the parent to avoid naming collisions.
  QString CreateUniqueFolderName(QStandardItem* folder_item,
                                 QStandardItem* parent);

  // Changes the visibility status of folders or scene icons.
  void SetIconVisibility(bool enable_visibility, QItemType item_type);
  void SetSceneIconVisibility(bool enable_visibility);
  void SetFolderIconVisibility(bool enable_visibility);

  // Retrieves and caches current video canvas base dimensions.
  void UpdateSceneSize();

  // Validates if the given scene conforms to cached canvas size requirements.
  bool IsManagedScene(obs_scene_t* scene) const;
  bool IsManagedScene(obs_source_t* scene_source) const;

  // Reorders items in the model by shifting the item at the index by the delta.
  bool MoveIndexByOne(const QModelIndex& index, int delta);

  // Sorts the children of the given folder according to its stored SortMode.
  void SortFolder(QStandardItem* folder);

  // Recursively sorts all folders starting from the given parent.
  void SortAllFolders(QStandardItem* parent);

  // Restores the folder's children to their stored user/manual order.
  void RestoreUserOrder(QStandardItem* folder);

  // Appends a child item details to its parent's kUserOrder metadata if sorted.
  void AddChildToUserOrder(QStandardItem* parent, QStandardItem* child);

 private:
  // Data payload representation for MIME drag-and-drop indexes.
  struct MimeItemData {
    QItemType type;
    void* data;  // Either QStandardItem* (if type == kFolder) or
                 // obs_weak_source_t* (if type == kScene)
  };

  // Comparer struct to compare weak source pointers by their strong references.
  struct SceneComp {
    bool operator()(obs_weak_source_t* x, obs_weak_source_t* y) const {
      return OBSGetStrongRef(x).Get() < OBSGetStrongRef(y).Get();
    }
  };

  using source_map_t =
      std::map<obs_weak_source_t*, QStandardItem*, SceneComp>;

  // Map of active OBS scene weak references to their corresponding UI item.
  source_map_t scenes_in_tree_;

  // Cached canvas size properties.
  SceneSize scene_size_;

  // Moves the scene standard item to a new parent row position.
  void MoveSceneItem(obs_weak_source_t* source, int row,
                     QStandardItem* parent_item);

  // Moves the folder item recursively to a new parent row position.
  void MoveSceneFolder(QStandardItem* item, int row,
                       QStandardItem* parent_item);

  // Creates and populates an array with serialized folder hierarchy details.
  obs_data_array_t* CreateFolderArray(QStandardItem& folder, QTreeView* view);

  // Serializes a single standard item (folder or scene) into an OBS config object.
  OBSDataAutoRelease SerializeItem(QStandardItem& item, QTreeView* view);

  // Parses and recreates folders and scene nodes from a configuration array.
  void LoadFolderArray(obs_data_array_t* folder_data, QStandardItem& folder,
                       std::list<StvFolderItem*>& expandable_folders);

  // Recursively applies the given icon to child nodes matching the item type.
  void SetIcon(const QIcon& icon, QItemType item_type, QStandardItem* item);
};

// Translates the given text key using OBS locale string lookup system.
inline QString QTStr(const char* text,
                     QMainWindow* main_window = reinterpret_cast<QMainWindow*>(
                         obs_frontend_get_main_window())) {
  return main_window->tr(text);
}

}  // namespace scene_tree_view

#endif  // OBS_SCENE_TREE_VIEW_STV_ITEM_MODEL_H_
