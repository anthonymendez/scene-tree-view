// Copyright (c) 2026 Anthony Mendez. All rights reserved.
// Use of this source code is governed by the GPL-2.0 license that can be
// found in the LICENSE file.

#include "obs_scene_tree_view/obs_scene_tree_view.h"

#include <QtWidgets/QDockWidget>
#include <QtWidgets/QMainWindow>
#include <obs-frontend-api.h>
#include <obs-module.h>
#include <util/platform.h>

#include "obs_scene_tree_view/version.h"

#define QT_TO_UTF8(str) str.toUtf8().constData()

OBS_DECLARE_MODULE();
OBS_MODULE_AUTHOR(PROJECT_AUTHOR);
OBS_MODULE_USE_DEFAULT_LOCALE(PROJECT_DATA_FOLDER, "en-US");

namespace scene_tree_view {
ObsSceneTreeView *g_stv_dock = nullptr;
bool g_stv_added = false;
} // namespace scene_tree_view

MODULE_EXPORT const char *obs_module_description(void) {
  return obs_module_text("Description");
}

MODULE_EXPORT const char *obs_module_name(void) {
  return obs_module_text("SceneTreeView");
}

MODULE_EXPORT bool obs_module_load(void) {
  blog(LOG_INFO, "[%s] loaded version %s", obs_module_name(), PROJECT_VERSION);

  BPtr<char> stv_config_path = obs_module_config_path("");
  if (!os_mkdir(stv_config_path)) {
    blog(LOG_WARNING, "[%s] failed to create config dir '%s'",
         obs_module_name(), stv_config_path.Get());
  }

  QMainWindow *main_window =
      reinterpret_cast<QMainWindow *>(obs_frontend_get_main_window());
  obs_frontend_push_ui_translation(obs_module_get_string);

  scene_tree_view::ObsSceneTreeView *view =
      new scene_tree_view::ObsSceneTreeView(main_window);
  view->setObjectName("obs_scene_tree_view");

  const char *t = obs_module_text("SceneTreeView.Title");
  QString title = QString::fromUtf8(t ? t : "");
  if (title.isEmpty() || title == QLatin1String("SceneTreeView.Title")) {
    title = QStringLiteral("Scene Tree View");
  }

  bool added = obs_frontend_add_dock_by_id("obs_scene_tree_view",
                                           QT_TO_UTF8(title), view);

  if (added) {
    blog(LOG_INFO, "[%s] registered via add_dock_by_id", obs_module_name());

    // Configure dock features on the OBS-created QDockWidget wrapper.
    if (auto *dockWidget = qobject_cast<QDockWidget *>(view->parentWidget())) {
      dockWidget->setAllowedAreas(Qt::AllDockWidgetAreas);
      dockWidget->setFeatures(QDockWidget::DockWidgetClosable |
                              QDockWidget::DockWidgetMovable |
                              QDockWidget::DockWidgetFloatable);
    }
  } else {
    blog(LOG_ERROR, "[%s] failed to register dock via add_dock_by_id",
         obs_module_name());
  }

  scene_tree_view::g_stv_dock = view;
  scene_tree_view::g_stv_added = added;
  obs_frontend_pop_ui_translation();

  return true;
}

MODULE_EXPORT void obs_module_unload() {}
