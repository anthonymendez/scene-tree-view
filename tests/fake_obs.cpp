// Copyright (c) 2026 Anthony Mendez. All rights reserved.
// Use of this source code is governed by the GPL-2.0 license that can be
// found in the LICENSE file.

#include "tests/fake_obs.h"

#include <obs-module.h>
#include <util/config-file.h>

#include <algorithm>
#include <cstdlib>
#include <map>
#include <memory>
#include <string>
#include <variant>
#include <vector>

struct obs_weak_source {
  obs_source *source = nullptr;
};

// Sources are never freed before Reset(), so a stale pointer held by the code
// under test stays readable; `alive` models whether the source still exists.
struct obs_source {
  std::string name;
  bool alive = true;
  obs_weak_source control;
};

struct obs_data_array {
  long refs = 1;
  std::vector<obs_data *> items;
};

struct obs_data {
  long refs = 1;
  std::map<std::string,
           std::variant<std::string, long long, bool, obs_data_array *>>
      values;
};

namespace {

std::vector<std::unique_ptr<obs_source>> g_all_sources;
std::vector<obs_source *> g_scene_list;
obs_source *g_current_scene = nullptr;

template <typename T>
const T *Find(obs_data_t *data, const char *name) {
  if (!data) {
    return nullptr;
  }
  auto it = data->values.find(name);
  return it == data->values.end() ? nullptr : std::get_if<T>(&it->second);
}

}  // namespace

namespace fake_obs {

void Reset() {
  g_scene_list.clear();
  g_all_sources.clear();
  g_current_scene = nullptr;
}

obs_source_t *CreateScene(const char *name) {
  auto source = std::make_unique<obs_source>();
  source->name = name;
  source->control.source = source.get();
  g_scene_list.push_back(source.get());
  g_all_sources.push_back(std::move(source));
  return g_scene_list.back();
}

void RemoveScene(obs_source_t *scene) {
  scene->alive = false;
  g_scene_list.erase(
      std::remove(g_scene_list.begin(), g_scene_list.end(), scene),
      g_scene_list.end());
  if (g_current_scene == scene) {
    g_current_scene = nullptr;
  }
}

void RenameScene(obs_source_t *scene, const char *name) { scene->name = name; }

void GetScenes(obs_frontend_source_list *list) {
  for (obs_source *scene : g_scene_list) {
    da_push_back(list->sources, &scene);
  }
}

obs_source_t *CurrentScene() { return g_current_scene; }

}  // namespace fake_obs

extern "C" {

// --- memory -----------------------------------------------------------------
void *bmalloc(size_t size) { return std::malloc(size); }
void *brealloc(void *ptr, size_t size) { return std::realloc(ptr, size); }
void bfree(void *ptr) { std::free(ptr); }

// --- logging / module -------------------------------------------------------
void blog(int, const char *, ...) {}
const char *obs_module_name(void) { return "stv_test"; }

// --- config -----------------------------------------------------------------
bool config_get_bool(config_t *, const char *, const char *) { return false; }
int64_t config_get_int(config_t *, const char *, const char *) { return 0; }

// --- sources and scenes -----------------------------------------------------
obs_source_t *obs_source_get_ref(obs_source_t *source) {
  return source && source->alive ? source : nullptr;
}
void obs_source_release(obs_source_t *) {}
const char *obs_source_get_name(const obs_source_t *source) {
  return source ? source->name.c_str() : nullptr;
}
obs_data_t *obs_source_get_settings(const obs_source_t *) {
  return obs_data_create();
}
obs_data_t *obs_source_get_private_settings(obs_source_t *) {
  return obs_data_create();
}

obs_weak_source_t *obs_source_get_weak_source(obs_source_t *source) {
  return source ? &source->control : nullptr;
}
void obs_weak_source_addref(obs_weak_source_t *) {}
void obs_weak_source_release(obs_weak_source_t *) {}
obs_source_t *obs_weak_source_get_source(obs_weak_source_t *weak) {
  return weak && weak->source->alive ? weak->source : nullptr;
}

obs_scene_t *obs_scene_from_source(const obs_source_t *source) {
  return reinterpret_cast<obs_scene_t *>(const_cast<obs_source_t *>(source));
}
obs_source_t *obs_scene_get_source(const obs_scene_t *scene) {
  return reinterpret_cast<obs_source_t *>(const_cast<obs_scene_t *>(scene));
}
void obs_scene_release(obs_scene_t *) {}
// obs_get_scene_by_name is an inline wrapper over this in obs.h.
obs_source_t *obs_get_source_by_name(const char *name) {
  for (obs_source *scene : g_scene_list) {
    if (scene->name == name) {
      return scene;
    }
  }
  return nullptr;
}

// --- frontend ---------------------------------------------------------------
void *obs_frontend_get_main_window(void) { return nullptr; }
config_t *obs_frontend_get_user_config(void) { return nullptr; }
config_t *obs_frontend_get_profile_config(void) { return nullptr; }
bool obs_frontend_preview_program_mode_active(void) { return false; }
obs_source_t *obs_frontend_get_current_scene(void) { return g_current_scene; }
void obs_frontend_set_current_scene(obs_source_t *scene) {
  g_current_scene = scene;
}
obs_source_t *obs_frontend_get_current_preview_scene(void) { return nullptr; }
void obs_frontend_set_current_preview_scene(obs_source_t *) {}

// --- obs_data ---------------------------------------------------------------
obs_data_t *obs_data_create() { return new obs_data(); }
void obs_data_addref(obs_data_t *data) {
  if (data) {
    ++data->refs;
  }
}
void obs_data_release(obs_data_t *data) {
  if (!data || --data->refs > 0) {
    return;
  }
  for (auto &[name, value] : data->values) {
    if (auto *array = std::get_if<obs_data_array *>(&value)) {
      obs_data_array_release(*array);
    }
  }
  delete data;
}

void obs_data_set_string(obs_data_t *data, const char *name, const char *val) {
  data->values[name] = std::string(val ? val : "");
}
void obs_data_set_int(obs_data_t *data, const char *name, long long val) {
  data->values[name] = val;
}
void obs_data_set_bool(obs_data_t *data, const char *name, bool val) {
  data->values[name] = val;
}
void obs_data_set_array(obs_data_t *data, const char *name,
                        obs_data_array_t *array) {
  obs_data_array_addref(array);
  data->values[name] = array;
}

const char *obs_data_get_string(obs_data_t *data, const char *name) {
  const auto *value = Find<std::string>(data, name);
  return value ? value->c_str() : "";
}
long long obs_data_get_int(obs_data_t *data, const char *name) {
  const auto *value = Find<long long>(data, name);
  return value ? *value : 0;
}
bool obs_data_get_bool(obs_data_t *data, const char *name) {
  const auto *value = Find<bool>(data, name);
  return value && *value;
}
obs_data_array_t *obs_data_get_array(obs_data_t *data, const char *name) {
  const auto *value = Find<obs_data_array *>(data, name);
  if (!value) {
    return nullptr;
  }
  obs_data_array_addref(*value);
  return *value;
}

obs_data_array_t *obs_data_array_create() { return new obs_data_array(); }
void obs_data_array_addref(obs_data_array_t *array) {
  if (array) {
    ++array->refs;
  }
}
void obs_data_array_release(obs_data_array_t *array) {
  if (!array || --array->refs > 0) {
    return;
  }
  for (obs_data *item : array->items) {
    obs_data_release(item);
  }
  delete array;
}
size_t obs_data_array_count(obs_data_array_t *array) {
  return array->items.size();
}
obs_data_t *obs_data_array_item(obs_data_array_t *array, size_t idx) {
  obs_data_addref(array->items[idx]);
  return array->items[idx];
}
size_t obs_data_array_push_back(obs_data_array_t *array, obs_data_t *obj) {
  obs_data_addref(obj);
  array->items.push_back(obj);
  return array->items.size() - 1;
}

}  // extern "C"
