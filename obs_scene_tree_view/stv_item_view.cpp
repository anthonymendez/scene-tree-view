// Copyright (c) 2026 Anthony Mendez. All rights reserved.
// Use of this source code is governed by the GPL-2.0 license that can be
// found in the LICENSE file.

#include "obs_scene_tree_view/stv_item_view.h"

#include <QMouseEvent>
#include <util/config-file.h>

namespace scene_tree_view {

StvItemView::StvItemView(QWidget* parent) : QTreeView(parent) {}

void StvItemView::SetItemModel(StvItemModel* model) { model_ = model; }

void StvItemView::selectionChanged(const QItemSelection& selected,
                                   const QItemSelection& deselected) {
  QTreeView::selectionChanged(selected, deselected);

  if (selected.indexes().empty()) {
    return;
  }

  assert(selected.indexes().size() == 1);
  QStandardItem* item = model_->itemFromIndex(selected.indexes().front());
  if (item->type() == StvItemModel::kScene) {
    model_->SetSelectedScene(item, obs_frontend_preview_program_mode_active());
  }
}

void StvItemView::EditSelectedItem() { edit(currentIndex()); }

void StvItemView::mouseDoubleClickEvent(QMouseEvent* event) {
  if (obs_frontend_preview_enabled()) {
    // If preview mode is enabled, check whether the option to transition output
    // scenes on double-click is active.
    const bool transition_enabled =
        config_get_bool(obs_frontend_get_app_config(), "BasicWindow",
                        "TransitionOnDoubleClick");

    if (transition_enabled) {
      QStandardItem* item = model_->itemFromIndex(indexAt(event->pos()));
      if (item && item->type() == StvItemModel::kScene) {
        model_->SetSelectedScene(item, /*set_preview_scene=*/false,
                                 /*force_set_scene=*/true);
        return;
      }
    }
  }

  // If TransitionOnDoubleClick is disabled or a folder is selected, perform a
  // normal edit on double click.
  QTreeView::mouseDoubleClickEvent(event);
}

}  // namespace scene_tree_view
