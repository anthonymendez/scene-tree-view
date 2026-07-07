// Copyright (c) 2026 Anthony Mendez. All rights reserved.
// Use of this source code is governed by the GPL-2.0 license that can be
// found in the LICENSE file.

#include "obs_scene_tree_view/stv_item_model.h"

#include <QLineEdit>
#include <QMessageBox>
#include <QMimeData>
#include <QRegularExpression>
#include <QtWidgets/QMainWindow>

#include <util/config-file.h>

namespace scene_tree_view {

StvFolderItem::StvFolderItem(const QString &text) : QStandardItem(text) {
  setDropEnabled(true);

  QMainWindow *main_window =
      reinterpret_cast<QMainWindow *>(obs_frontend_get_main_window());
  QIcon icon = config_get_bool(obs_frontend_get_user_config(), "SceneTreeView",
                               "ShowFolderIcons")
                   ? main_window->property("groupIcon").value<QIcon>()
                   : QIcon();
  setIcon(icon);
}

int StvFolderItem::type() const { return StvItemModel::kFolder; }

StvSceneItem::StvSceneItem(const QString &text, obs_weak_source_t *weak)
    : QStandardItem(text) {
  setDropEnabled(false);
  setData(QVariant::fromValue(ObsWeakSourcePtr({weak})),
          StvItemModel::kObsScene);

  QMainWindow *main_window =
      reinterpret_cast<QMainWindow *>(obs_frontend_get_main_window());
  QIcon icon = config_get_bool(obs_frontend_get_user_config(), "SceneTreeView",
                               "ShowSceneIcons")
                   ? main_window->property("sceneIcon").value<QIcon>()
                   : QIcon();
  setIcon(icon);
}

int StvSceneItem::type() const { return StvItemModel::kScene; }

StvItemModel::StvItemModel() {}

StvItemModel::~StvItemModel() {
  // Remove scene refs.
  for (auto &scene : scenes_in_tree_) {
    obs_weak_source_release(scene.first);
  }
  scenes_in_tree_.clear();
}

QStringList StvItemModel::mimeTypes() const {
  return QStringList(kMimeType.data());
}

QMimeData *StvItemModel::mimeData(const QModelIndexList &indexes) const {
  QMimeData *mime = new QMimeData();

  const int num_indexes = indexes.size();

  QByteArray mime_dat;
  mime_dat.reserve(num_indexes * sizeof(MimeItemData) + sizeof(int));
  mime_dat.append((const char *)&num_indexes, sizeof(int));

  for (const auto &index : indexes) {
    MimeItemData item_mime_dat;
    const QStandardItem *item = itemFromIndex(index);

    assert(item->type() == kFolder || item->type() == kScene);
    item_mime_dat.type = (QItemType)item->type();
    item_mime_dat.data =
        item_mime_dat.type == QItemType::kFolder
            ? (void *)item
            : (void *)item->data(kObsScene).value<ObsWeakSourcePtr>().ptr;

    mime_dat.append((const char *)&item_mime_dat, sizeof(MimeItemData));
  }

  mime->setData(kMimeType.data(), mime_dat);

  return mime;
}

bool StvItemModel::dropMimeData(const QMimeData *data, Qt::DropAction action,
                                int row, int column,
                                const QModelIndex &parent) {
  Q_UNUSED(action);
  Q_UNUSED(column);

  QStandardItem *parent_item = itemFromIndex(parent);
  if (!parent_item) {
    parent_item = invisibleRootItem();
  } else if (parent_item->type() == QItemType::kScene) {
    return false;
  }

  if (parent_item->data(kSortMode).toInt() !=
      static_cast<int>(SortMode::kUser)) {
    parent_item->setData(static_cast<int>(SortMode::kUser), kSortMode);
  }

  if (row < 0) {
    row = 0;
  }

  QByteArray qdat = data->data(kMimeType.data());
  assert(qdat.size() >= (int)sizeof(int));

  const char *dat = qdat.constData();

  const int num_indexes = *(int *)dat;
  dat += sizeof(int);

  for (int i = 0; i < num_indexes; ++i) {
    // Find item and move it.
    const MimeItemData *item_data = (const MimeItemData *)dat;
    assert(item_data->type == kFolder || item_data->type == kScene);
    if (item_data->type == kScene) {
      MoveSceneItem((obs_weak_source_t *)item_data->data, row + i, parent_item);
    } else {
      MoveSceneFolder((QStandardItem *)item_data->data, row + i, parent_item);
    }

    dat += sizeof(MimeItemData);
  }

  return true;
}

void StvItemModel::UpdateTree(obs_frontend_source_list &scene_list,
                              const QModelIndex &selected_index) {
  UpdateSceneSize();

  source_map_t new_scene_tree;

  for (size_t i = 0; i < scene_list.sources.num; i++) {
    obs_source_t *source = scene_list.sources.array[i];
    assert(obs_scene_from_source(source) != nullptr);

    if (!IsManagedScene(source)) {
      continue;
    }

    source_map_t::iterator scene_it;

    // Check if scene already in tree.
    obs_weak_source_t *weak = obs_source_get_weak_source(source);

    scene_it = scenes_in_tree_.find(weak);
    if (scene_it != scenes_in_tree_.end()) {
      // If already in tree, move to new set.
      auto new_scene_it =
          new_scene_tree.emplace(scene_it->first, scene_it->second).first;
      scenes_in_tree_.erase(scene_it);
      scene_it = new_scene_it;

      obs_weak_source_release(weak);
    } else {
      QStandardItem *pending_item = FindPendingSceneItem(
          invisibleRootItem(), obs_source_get_name(source));
      if (pending_item) {
        pending_item->setData(QVariant::fromValue(ObsWeakSourcePtr({weak})),
                              kObsScene);
        scene_it = new_scene_tree.emplace(weak, pending_item).first;

        QMainWindow *main_window =
            reinterpret_cast<QMainWindow *>(obs_frontend_get_main_window());
        QIcon icon = config_get_bool(obs_frontend_get_user_config(),
                                     "SceneTreeView", "ShowSceneIcons")
                         ? main_window->property("sceneIcon").value<QIcon>()
                         : QIcon();
        pending_item->setIcon(icon);
      } else {
        scene_it = new_scene_tree.emplace(weak, nullptr).first;
      }
    }

    weak = nullptr;

    // Check that scene contains tree data.
    obs_data_t *scene_dat =
        obs_source_get_private_settings(OBSGetStrongRef(scene_it->first).Get());

    if (!scene_it->second) {
      // Scene not yet in tree, add it at the correct position.
      QStandardItem *selected = itemFromIndex(selected_index);
      QStandardItem *parent;
      if (selected) {
        assert(selected->type() == QItemType::kScene ||
               selected->type() == QItemType::kFolder);

        if (selected->type() == QItemType::kFolder) {
          parent = selected;
        } else {
          parent = GetParentOrRoot(selected->index());
        }
      } else {
        selected = invisibleRootItem();
        parent = selected;
      }

      // Add new item to scene.
      StvSceneItem *pItem =
          new StvSceneItem(obs_source_get_name(source), scene_it->first);

      const auto row = parent == selected ? 0 : selected->row();
      parent->insertRow(row, pItem);
      AddChildToUserOrder(parent, pItem);

      scene_it->second = pItem;
    } else {
      // Update scene name.
      scene_it->second->setText(obs_source_get_name(source));
    }

    obs_data_release(scene_dat);
    scene_dat = nullptr;
  }

  // Erase all remaining elements in scenes_in_tree_.
  for (const auto &scene : scenes_in_tree_) {
    assert(scene.second);

    const int row = scene.second->row();
    removeRow(row, parent(scene.second->index()));

    // Remove scene reference.
    obs_weak_source_release(scene.first);
  }

  scenes_in_tree_ = std::move(new_scene_tree);
  SortAllFolders(invisibleRootItem());
}

bool StvItemModel::CheckFolderNameUniqueness(const QString &name,
                                             QStandardItem *parent,
                                             QStandardItem *item_to_skip) {
  const int row_count = parent->rowCount();
  for (int i = 0; i < row_count; ++i) {
    QStandardItem *item = parent->child(i);
    if (item == item_to_skip) {
      continue;
    }

    if (item->type() == kFolder && item->text() == name) {
      return false;
    }
  }

  return true;
}

void StvItemModel::SetSelectedScene(QStandardItem *item, bool set_preview_scene,
                                    bool force_set_scene) {
  obs_weak_source_t *weak =
      item->data(QDataRole::kObsScene).value<ObsWeakSourcePtr>().ptr;
  OBSSourceAutoRelease source = OBSGetStrongRef(weak);
  if (source) {
    if (!set_preview_scene) {
      if (force_set_scene ||
          OBSSourceAutoRelease(obs_frontend_get_current_scene()).Get() !=
              source) {
        obs_frontend_set_current_scene(source);
      }
    } else if (force_set_scene ||
               OBSSourceAutoRelease(obs_frontend_get_current_preview_scene())
                       .Get() != source) {
      obs_frontend_set_current_preview_scene(source);
    }
  }
}

QStandardItem *StvItemModel::GetCurrentSceneItem() {
  // Change source to the selected one.
  OBSSourceAutoRelease source = GetCurrentScene();
  OBSWeakSource weak = OBSGetWeakRef(source);

  if (auto scene_it = scenes_in_tree_.find(weak);
      scene_it != scenes_in_tree_.end()) {
    return scene_it->second;
  } else {
    blog(LOG_WARNING, "[%s] Couldn't find current scene in Scene Tree View",
         obs_module_name());
    return nullptr;
  }
}

OBSSourceAutoRelease StvItemModel::GetCurrentScene() {
  return obs_frontend_preview_program_mode_active()
             ? obs_frontend_get_current_preview_scene()
             : obs_frontend_get_current_scene();
}

void StvItemModel::SaveSceneTree(obs_data_t *root_folder_data,
                                 const char *scene_collection,
                                 QTreeView *view) {
  OBSDataArrayAutoRelease folder_data =
      CreateFolderArray(*invisibleRootItem(), view);
  obs_data_set_array(root_folder_data, scene_collection, folder_data);

  int root_sort_mode = invisibleRootItem()->data(kSortMode).toInt();
  std::string root_sort_key = std::string(scene_collection) + "_sort_mode";
  obs_data_set_int(root_folder_data, root_sort_key.c_str(), root_sort_mode);
}

void StvItemModel::LoadSceneTree(obs_data_t *root_folder_data,
                                 const char *scene_collection) {
  UpdateSceneSize();

  QStandardItem *root_item = invisibleRootItem();

  // Erase previous data.
  CleanupSceneTree();

  std::string root_sort_key = std::string(scene_collection) + "_sort_mode";
  int root_sort_mode =
      obs_data_get_int(root_folder_data, root_sort_key.c_str());
  root_item->setData(root_sort_mode, kSortMode);

  // Add loaded data.
  OBSDataArrayAutoRelease folder_array =
      obs_data_get_array(root_folder_data, scene_collection);
  if (folder_array) {
    LoadFolderArray(folder_array, *root_item);
  }

  if (root_sort_mode == static_cast<int>(SortMode::kAlphaAsc) ||
      root_sort_mode == static_cast<int>(SortMode::kAlphaDesc)) {
    QVariantList user_list;
    for (int j = 0; j < root_item->rowCount(); ++j) {
      QStandardItem *child = root_item->child(j);
      QVariantMap map;
      map["name"] = child->text();
      map["type"] = child->type();
      user_list.append(map);
    }
    root_item->setData(user_list, kUserOrder);
  }

  SortAllFolders(root_item);
}

void StvItemModel::CleanupSceneTree() {
  // Remove scene refs.
  for (auto &scene : scenes_in_tree_) {
    obs_weak_source_release(scene.first);
  }

  scenes_in_tree_.clear();

  QStandardItem *root_item = invisibleRootItem();
  root_item->removeRows(0, root_item->rowCount());
}

QStandardItem *StvItemModel::GetParentOrRoot(const QModelIndex &index) {
  QStandardItem *selected = itemFromIndex(parent(index));
  if (!selected) {
    selected = invisibleRootItem();
  }

  return selected;
}

QString StvItemModel::CreateUniqueFolderName(QStandardItem *folder_item,
                                             QStandardItem *parent) {
  // Check that name is unique.
  QString folder_name = folder_item->text();
  if (!CheckFolderNameUniqueness(folder_name, parent, folder_item)) {
    QString format = folder_name.replace(QRegularExpression("\\d+$"), "%1");
    if (!format.endsWith("%1")) {
      format += " %1";
    }

    size_t i = 0;
    QString name;
    do {
      name = format.arg(QString::number(++i));
    } while (!CheckFolderNameUniqueness(name, parent, folder_item));

    folder_name = name;
  }

  return folder_name;
}

void StvItemModel::SetIconVisibility(bool enable_visibility,
                                     QItemType item_type) {
  if (item_type == kScene) {
    SetSceneIconVisibility(enable_visibility);
  } else {
    SetFolderIconVisibility(enable_visibility);
  }
}

void StvItemModel::SetSceneIconVisibility(bool enable_visibility) {
  QMainWindow *main_window =
      reinterpret_cast<QMainWindow *>(obs_frontend_get_main_window());
  QIcon icon = enable_visibility
                   ? main_window->property("sceneIcon").value<QIcon>()
                   : QIcon();

  SetIcon(icon, kScene, invisibleRootItem());
}

void StvItemModel::SetFolderIconVisibility(bool enable_visibility) {
  QMainWindow *main_window =
      reinterpret_cast<QMainWindow *>(obs_frontend_get_main_window());
  QIcon icon = enable_visibility
                   ? main_window->property("groupIcon").value<QIcon>()
                   : QIcon();

  SetIcon(icon, kFolder, invisibleRootItem());
}

void StvItemModel::UpdateSceneSize() {
  scene_size_.cx =
      config_get_int(obs_frontend_get_profile_config(), "Video", "BaseCX");
  scene_size_.cy =
      config_get_int(obs_frontend_get_profile_config(), "Video", "BaseCY");
}

bool StvItemModel::IsManagedScene(obs_scene_t *scene) const {
  OBSSource source = obs_scene_get_source(scene);
  return IsManagedScene(source);
}

bool StvItemModel::IsManagedScene(obs_source_t *scene_source) const {
  OBSDataAutoRelease settings = obs_source_get_settings(scene_source);
  return obs_data_get_bool(settings, "custom_size") == false;
}

bool StvItemModel::MoveIndexByOne(const QModelIndex &index, int delta) {
  if (!index.isValid()) {
    return false;
  }

  QStandardItem *item = itemFromIndex(index);
  if (!item) {
    return false;
  }

  QStandardItem *parent_item = itemFromIndex(index.parent());
  if (!parent_item) {
    parent_item = invisibleRootItem();
  }

  if (parent_item->data(kSortMode).toInt() !=
      static_cast<int>(SortMode::kUser)) {
    parent_item->setData(static_cast<int>(SortMode::kUser), kSortMode);
  }

  const int row = index.row();
  const int rowCount = parent_item->rowCount();
  int insertPos = row + delta;
  // For moving down, insert after the next item (to land at row+1 after
  // removal).
  if (delta > 0) {
    insertPos++;
  }
  // Allow insert at end (insertPos == rowCount) but not beyond.
  if (insertPos < 0 || insertPos > rowCount) {
    return false;
  }

  if (item->type() == kScene) {
    obs_weak_source_t *weak =
        item->data(kObsScene).value<ObsWeakSourcePtr>().ptr;
    MoveSceneItem(weak, insertPos, parent_item);
  } else if (item->type() == kFolder) {
    MoveSceneFolder(item, insertPos, parent_item);
  } else {
    return false;
  }

  // Remove the original row. When inserting below (delta>0), original stays at
  // 'row'. When inserting above (delta<0), original shifts down to 'row+1'.
  if (delta > 0) {
    parent_item->removeRow(row);
  } else {
    parent_item->removeRow(row + 1);
  }

  return true;
}

void StvItemModel::MoveSceneItem(obs_weak_source_t *source, int row,
                                 QStandardItem *parent_item) {
  if (const auto scene_it = scenes_in_tree_.find(source);
      scene_it != scenes_in_tree_.end()) {
    assert(scene_it->second->type() == kScene);

    blog(LOG_INFO, "[%s] Moving %s", obs_module_name(),
         scene_it->second->text().toStdString().c_str());

    StvSceneItem *pItem =
        new StvSceneItem(scene_it->second->text(), scene_it->first);
    parent_item->insertRow(row, pItem);
    AddChildToUserOrder(parent_item, pItem);

    // Old item removed when returning true.

    scene_it->second = pItem;
  } else {
    blog(LOG_WARNING, "[%s] Couldn't find item to move in Scene Tree View",
         obs_module_name());
  }
}

void StvItemModel::MoveSceneFolder(QStandardItem *item, int row,
                                   QStandardItem *parent_item) {
  assert(item->type() == kFolder);
  blog(LOG_INFO, "[%s] Moving %s", obs_module_name(),
       item->text().toStdString().c_str());

  // Check that name is unique.
  QString new_name = CreateUniqueFolderName(item, parent_item);

  StvFolderItem *new_item = new StvFolderItem(new_name);
  new_item->setData(item->data(kSortMode), kSortMode);
  new_item->setData(item->data(kUserOrder), kUserOrder);

  parent_item->insertRow(row, new_item);
  AddChildToUserOrder(parent_item, new_item);

  for (int sub_row = 0; sub_row < item->rowCount(); ++sub_row) {
    QStandardItem *sub_item = item->child(sub_row);

    assert(sub_item->type() == kFolder || sub_item->type() == kScene);

    if (sub_item->type() == kFolder) {
      MoveSceneFolder(sub_item, sub_row, new_item);
    } else {
      obs_weak_source_t *weak =
          sub_item->data(kObsScene).value<ObsWeakSourcePtr>().ptr;
      MoveSceneItem(weak, sub_row, new_item);
    }
  }
}

OBSDataAutoRelease StvItemModel::SerializeItem(QStandardItem &item,
                                               QTreeView *view) {
  OBSDataAutoRelease item_data = obs_data_create();
  if (item.type() == kFolder) {
    OBSDataArrayAutoRelease sub_folder_data = CreateFolderArray(item, view);
    obs_data_set_array(item_data, kSceneTreeConfigFolderData.data(),
                       sub_folder_data);
    obs_data_set_bool(item_data, kSceneTreeConfigFolderExpanded.data(),
                      item.data(kExpanded).toBool());
    obs_data_set_string(item_data, kSceneTreeConfigItemNameData.data(),
                        item.text().toStdString().c_str());

    int sort_mode = item.data(kSortMode).toInt();
    obs_data_set_int(item_data, "sort_mode", sort_mode);
  } else {
    obs_weak_source_t *weak =
        item.data(QDataRole::kObsScene).value<ObsWeakSourcePtr>().ptr;
    if (!weak) {
      // Placeholder scene not yet resolved — serialize using stored text name
      // to avoid losing its position in the hierarchy.
      obs_data_set_string(item_data, kSceneTreeConfigItemNameData.data(),
                          item.text().toStdString().c_str());
    } else {
      OBSSourceAutoRelease source = OBSGetStrongRef(weak);
      obs_data_set_string(item_data, kSceneTreeConfigItemNameData.data(),
                          obs_source_get_name(source));
    }
  }
  return item_data;
}

obs_data_array_t *StvItemModel::CreateFolderArray(QStandardItem &folder,
                                                  QTreeView *view) {
  obs_data_array_t *folder_data = obs_data_array_create();

  int sort_mode = folder.data(kSortMode).toInt();
  QVariant var_user_order = folder.data(kUserOrder);

  if ((sort_mode == static_cast<int>(SortMode::kAlphaAsc) ||
       sort_mode == static_cast<int>(SortMode::kAlphaDesc)) &&
      var_user_order.isValid() && !var_user_order.isNull()) {
    QVariantList user_list = var_user_order.toList();
    QList<QStandardItem *> children;
    for (int i = 0; i < folder.rowCount(); ++i) {
      children.append(folder.child(i));
    }

    for (const QVariant &v : user_list) {
      QVariantMap map = v.toMap();
      QString name = map.value("name").toString();
      int type = map.value("type").toInt();

      for (int i = 0; i < children.size(); ++i) {
        QStandardItem *item = children.at(i);
        if (item && item->text() == name && item->type() == type) {
          OBSDataAutoRelease item_data = SerializeItem(*item, view);
          obs_data_array_push_back(folder_data, item_data);
          children.removeAt(i);
          break;
        }
      }
    }

    for (QStandardItem *item : children) {
      OBSDataAutoRelease item_data = SerializeItem(*item, view);
      obs_data_array_push_back(folder_data, item_data);
    }
  } else {
    for (int i = 0; i < folder.rowCount(); ++i) {
      QStandardItem *item = folder.child(i);
      OBSDataAutoRelease item_data = SerializeItem(*item, view);
      obs_data_array_push_back(folder_data, item_data);
    }
  }

  return folder_data;
}

void StvItemModel::LoadFolderArray(obs_data_array_t *folder_data,
                                   QStandardItem &folder) {
  const size_t item_count = obs_data_array_count(folder_data);
  for (size_t i = 0; i < item_count; ++i) {
    OBSDataAutoRelease item_data = obs_data_array_item(folder_data, i);

    const char *item_name =
        obs_data_get_string(item_data, kSceneTreeConfigItemNameData.data());
    OBSDataArrayAutoRelease sub_folder_data =
        obs_data_get_array(item_data, kSceneTreeConfigFolderData.data());

    // Check if this is folder or scene item (only folders have
    // sub_folder_data).
    if (!sub_folder_data) {
      OBSSceneAutoRelease scene = obs_get_scene_by_name(item_name);
      obs_weak_source_t *weak = nullptr;
      if (scene && IsManagedScene(scene)) {
        OBSSource source = obs_scene_get_source(scene);
        weak = obs_source_get_weak_source(source);

        // Skip if scene already in treeview.
        // (see issue
        // https://github.com/DigitOtter/obs_scene_tree_view/issues/19)
        if (scenes_in_tree_.find(weak) != scenes_in_tree_.end()) {
          obs_weak_source_release(weak);
          continue;
        }
      }

      StvSceneItem *new_scene_item = new StvSceneItem(item_name, weak);
      folder.appendRow(new_scene_item);

      if (weak) {
        scenes_in_tree_.emplace(weak, new_scene_item);
      }
    } else {
      StvFolderItem *new_folder_item = new StvFolderItem(item_name);
      int sort_mode = obs_data_get_int(item_data, "sort_mode");
      new_folder_item->setData(sort_mode, kSortMode);

      LoadFolderArray(sub_folder_data, *new_folder_item);

      if (sort_mode == static_cast<int>(SortMode::kAlphaAsc) ||
          sort_mode == static_cast<int>(SortMode::kAlphaDesc)) {
        QVariantList user_list;
        for (int j = 0; j < new_folder_item->rowCount(); ++j) {
          QStandardItem *child = new_folder_item->child(j);
          QVariantMap map;
          map["name"] = child->text();
          map["type"] = child->type();
          user_list.append(map);
        }
        new_folder_item->setData(user_list, kUserOrder);
      }

      folder.appendRow(new_folder_item);

      // Read and store expansion state in model data
      bool expanded =
          obs_data_get_bool(item_data, kSceneTreeConfigFolderExpanded.data());
      new_folder_item->setData(expanded, kExpanded);
    }
  }
}

void StvItemModel::SetIcon(const QIcon &icon, QItemType item_type,
                           QStandardItem *item) {
  if (!item) {
    return;
  }

  for (int i = 0; i < item->rowCount(); ++i) {
    QStandardItem *child = item->child(i);
    if (child->type() == item_type) {
      child->setIcon(icon);
    }

    if (child->type() == kFolder) {
      SetIcon(icon, item_type, child);
    }
  }
}

void StvItemModel::SortFolder(QStandardItem *folder) {
  if (!folder) {
    return;
  }

  int mode_val = folder->data(kSortMode).toInt();
  SortMode mode = static_cast<SortMode>(mode_val);

  if (mode == SortMode::kAlphaAsc) {
    folder->sortChildren(0, Qt::AscendingOrder);
  } else if (mode == SortMode::kAlphaDesc) {
    folder->sortChildren(0, Qt::DescendingOrder);
  }
}

void StvItemModel::SortAllFolders(QStandardItem *parent) {
  if (!parent) {
    return;
  }

  SortFolder(parent);

  for (int i = 0; i < parent->rowCount(); ++i) {
    QStandardItem *child = parent->child(i);
    if (child && child->type() == kFolder) {
      SortAllFolders(child);
    }
  }
}

void StvItemModel::RestoreUserOrder(QStandardItem *folder) {
  if (!folder) {
    return;
  }

  QVariant var = folder->data(kUserOrder);
  if (!var.isValid() || var.isNull()) {
    return;
  }

  QVariantList list = var.toList();
  if (list.isEmpty()) {
    return;
  }

  QList<QStandardItem *> taken;
  while (folder->rowCount() > 0) {
    taken.append(folder->takeRow(0).at(0));
  }

  for (const QVariant &v : list) {
    QVariantMap map = v.toMap();
    QString name = map.value("name").toString();
    int type = map.value("type").toInt();

    for (int i = 0; i < taken.size(); ++i) {
      QStandardItem *item = taken.at(i);
      if (item && item->text() == name && item->type() == type) {
        folder->appendRow(item);
        taken.removeAt(i);
        break;
      }
    }
  }

  for (QStandardItem *item : taken) {
    folder->appendRow(item);
  }
}

void StvItemModel::AddChildToUserOrder(QStandardItem *parent,
                                       QStandardItem *child) {
  if (!parent || !child) {
    return;
  }

  int sort_mode = parent->data(kSortMode).toInt();
  if (sort_mode == static_cast<int>(SortMode::kAlphaAsc) ||
      sort_mode == static_cast<int>(SortMode::kAlphaDesc)) {
    QVariant var = parent->data(kUserOrder);
    if (var.isValid() && !var.isNull()) {
      QVariantList user_list = var.toList();
      QVariantMap map;
      map["name"] = child->text();
      map["type"] = child->type();
      user_list.append(map);
      parent->setData(user_list, kUserOrder);
    }
  }
}

QStandardItem *StvItemModel::FindPendingSceneItem(QStandardItem *parent,
                                                  const QString &name) {
  if (!parent) {
    return nullptr;
  }

  for (int i = 0; i < parent->rowCount(); ++i) {
    QStandardItem *child = parent->child(i);
    if (!child) {
      continue;
    }

    if (child->type() == kScene) {
      obs_weak_source_t *weak =
          child->data(QDataRole::kObsScene).value<ObsWeakSourcePtr>().ptr;
      if (weak == nullptr && child->text() == name) {
        return child;
      }
    } else if (child->type() == kFolder) {
      QStandardItem *found = FindPendingSceneItem(child, name);
      if (found) {
        return found;
      }
    }
  }

  return nullptr;
}

} // namespace scene_tree_view
