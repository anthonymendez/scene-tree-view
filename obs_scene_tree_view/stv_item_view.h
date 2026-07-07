// Copyright (c) 2026 Anthony Mendez. All rights reserved.
// Use of this source code is governed by the GPL-2.0 license that can be
// found in the LICENSE file.

#ifndef OBS_SCENE_TREE_VIEW_STV_ITEM_VIEW_H_
#define OBS_SCENE_TREE_VIEW_STV_ITEM_VIEW_H_

#include <QtWidgets/QTreeView>

#include "obs_scene_tree_view/stv_item_model.h"

namespace scene_tree_view {

// Custom tree view component that displays scene and folder items.
// Coordinates selection change events with the model to synchronize active OBS
// scenes and processes double-clicks for renaming or immediate transition.
class StvItemView : public QTreeView {
  Q_OBJECT

public:
  // Creates a new scene tree item view.
  explicit StvItemView(QWidget *parent = nullptr);
  ~StvItemView() override = default;

  // Associates this view with the provided item model.
  void SetItemModel(StvItemModel *model);

protected slots:
  // Handles selection changes to update active scene in OBS.
  void selectionChanged(const QItemSelection &selected,
                        const QItemSelection &deselected) override;

  // Initiates renaming editor for the currently selected item.
  void EditSelectedItem();

  // Handles mouse double-click events to trigger rename or transition.
  void mouseDoubleClickEvent(QMouseEvent *event) override;

private:
  // Pointer to the underlying scene tree item model.
  StvItemModel *model_ = nullptr;
};

} // namespace scene_tree_view

#endif // OBS_SCENE_TREE_VIEW_STV_ITEM_VIEW_H_
