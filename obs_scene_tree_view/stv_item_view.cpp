// Copyright (c) 2026 Anthony Mendez. All rights reserved.
// Use of this source code is governed by the GPL-2.0 license that can be
// found in the LICENSE file.

#include "obs_scene_tree_view/stv_item_view.h"

#include <QKeyEvent>
#include <QMouseEvent>
#include <util/config-file.h>

namespace scene_tree_view {

StvItemView::StvItemView(QWidget *parent) : QTreeView(parent) {
  setSelectionMode(QAbstractItemView::ExtendedSelection);
}

void StvItemView::SetItemModel(StvItemModel *model) { model_ = model; }

void StvItemView::selectionChanged(const QItemSelection &selected,
                                   const QItemSelection &deselected) {
  QTreeView::selectionChanged(selected, deselected);

  QModelIndex curr = currentIndex();
  if (!curr.isValid()) {
    return;
  }

  QStandardItem *item = model_->itemFromIndex(curr);
  if (item && item->type() == StvItemModel::kScene) {
    model_->SetSelectedScene(item, obs_frontend_preview_program_mode_active());
  }
}

void StvItemView::EditSelectedItem() { edit(currentIndex()); }

void StvItemView::mouseDoubleClickEvent(QMouseEvent *event) {
  if (obs_frontend_preview_enabled()) {
    // If preview mode is enabled, check whether the option to transition output
    // scenes on double-click is active.
    const bool transition_enabled =
        config_get_bool(obs_frontend_get_app_config(), "BasicWindow",
                        "TransitionOnDoubleClick");

    if (transition_enabled) {
      QStandardItem *item = model_->itemFromIndex(indexAt(event->pos()));
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

void StvItemView::keyPressEvent(QKeyEvent *event) {
  if (event->key() == Qt::Key_Delete || event->key() == Qt::Key_Backspace) {
    emit deletePressed();
    event->accept();
    return;
  }
  QTreeView::keyPressEvent(event);
}

} // namespace scene_tree_view
