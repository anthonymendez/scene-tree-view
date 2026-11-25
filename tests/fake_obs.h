// Copyright (c) 2026 Anthony Mendez. All rights reserved.
// Use of this source code is governed by the GPL-2.0 license that can be
// found in the LICENSE file.

#ifndef TESTS_FAKE_OBS_H_
#define TESTS_FAKE_OBS_H_

#include <obs-frontend-api.h>
#include <obs.h>

// In-memory stand-in for the parts of libobs and the OBS frontend API that the
// scene tree model calls. The test binary links this instead of libobs, so the
// model can be exercised without a running OBS.
namespace fake_obs {

// Drops every scene and resets the current scene. Call between tests.
void Reset();

// Adds a scene to the fake OBS scene list and returns it.
obs_source_t *CreateScene(const char *name);

// Removes a scene from the fake OBS scene list; weak references to it expire.
void RemoveScene(obs_source_t *scene);

// Renames a scene in place.
void RenameScene(obs_source_t *scene, const char *name);

// Fills `list` with the current scenes in order, as obs_frontend_get_scenes
// does. Free it with obs_frontend_source_list_free.
void GetScenes(obs_frontend_source_list *list);

// Returns the scene last passed to obs_frontend_set_current_scene.
obs_source_t *CurrentScene();

}  // namespace fake_obs

#endif  // TESTS_FAKE_OBS_H_
