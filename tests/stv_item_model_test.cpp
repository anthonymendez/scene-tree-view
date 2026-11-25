// Copyright (c) 2026 Anthony Mendez. All rights reserved.
// Use of this source code is governed by the GPL-2.0 license that can be
// found in the LICENSE file.

// Baseline tests for StvItemModel. They record how the model behaves today so
// that later changes to moving, sorting and saving show up as deliberate test
// edits. Where the current behaviour is a known quirk, the test says so.

// QtTest must come before the OBS headers: it pulls in the compiler's SIMD
// intrinsics, which libobs's SIMDe headers otherwise redefine.
// clang-format off
#include <QtTest>
// clang-format on

#include "obs_scene_tree_view/stv_item_model.h"

#include <QMimeData>
#include <QStringList>

#include "tests/fake_obs.h"

namespace scene_tree_view {
namespace {

constexpr char kCollection[] = "collection";

// Renders a subtree as "Folder[child,child],scene".
QString Dump(const QStandardItem *parent) {
  QStringList parts;
  for (int i = 0; i < parent->rowCount(); ++i) {
    const QStandardItem *child = parent->child(i);
    if (child->type() == StvItemModel::kFolder) {
      parts << child->text() + "[" + Dump(child) + "]";
    } else {
      parts << child->text();
    }
  }
  return parts.join(",");
}

QString Dump(const StvItemModel &model) {
  return Dump(model.invisibleRootItem());
}

void Sync(StvItemModel &model, const QModelIndex &selected = QModelIndex()) {
  obs_frontend_source_list scenes = {};
  fake_obs::GetScenes(&scenes);
  model.UpdateTree(scenes, selected);
  obs_frontend_source_list_free(&scenes);
}

obs_weak_source_t *WeakOf(const QStandardItem *item) {
  return item->data(StvItemModel::kObsScene).value<ObsWeakSourcePtr>().ptr;
}

obs_data_t *SceneEntry(const char *name) {
  obs_data_t *entry = obs_data_create();
  obs_data_set_string(entry, "name", name);
  return entry;
}

obs_data_t *FolderEntry(const char *name, const QStringList &scenes,
                        long long sort_mode = 0, bool expanded = false) {
  obs_data_t *entry = obs_data_create();
  obs_data_set_string(entry, "name", name);
  obs_data_set_int(entry, "sort_mode", sort_mode);
  obs_data_set_bool(entry, "is_expanded", expanded);
  OBSDataArrayAutoRelease children = obs_data_array_create();
  for (const QString &scene : scenes) {
    OBSDataAutoRelease child = SceneEntry(scene.toUtf8().constData());
    obs_data_array_push_back(children, child);
  }
  obs_data_set_array(entry, "folder", children);
  return entry;
}

// Builds the saved-tree object LoadSceneTree reads. Takes ownership of entries.
OBSDataAutoRelease SavedTree(std::initializer_list<obs_data_t *> entries) {
  OBSDataAutoRelease root = obs_data_create();
  OBSDataArrayAutoRelease array = obs_data_array_create();
  for (obs_data_t *entry : entries) {
    obs_data_array_push_back(array, entry);
    obs_data_release(entry);
  }
  obs_data_set_array(root, kCollection, array);
  return root;
}

class StvItemModelTest : public QObject {
  Q_OBJECT

 private slots:
  void cleanup() { fake_obs::Reset(); }

  // Known quirk: each new scene is inserted at row 0, so a tree built from
  // scratch lists scenes in the reverse of OBS's order.
  void UpdateTreeAddsNewScenesAtTopOfRoot() {
    fake_obs::CreateScene("A");
    fake_obs::CreateScene("B");
    fake_obs::CreateScene("C");
    StvItemModel model;
    Sync(model);
    QCOMPARE(Dump(model), QString("C,B,A"));
  }

  void UpdateTreeInsertsNewSceneIntoSelectedFolder() {
    fake_obs::CreateScene("A");
    StvItemModel model;
    OBSDataAutoRelease saved = SavedTree({FolderEntry("F", {"A"})});
    model.LoadSceneTree(saved, kCollection);

    fake_obs::CreateScene("B");
    Sync(model, model.index(0, 0));
    QCOMPARE(Dump(model), QString("F[B,A]"));
  }

  void UpdateTreeRemovesDeletedScene() {
    fake_obs::CreateScene("A");
    obs_source_t *b = fake_obs::CreateScene("B");
    StvItemModel model;
    Sync(model);

    fake_obs::RemoveScene(b);
    Sync(model);
    QCOMPARE(Dump(model), QString("A"));
  }

  void UpdateTreeRenamesScene() {
    obs_source_t *a = fake_obs::CreateScene("A");
    StvItemModel model;
    Sync(model);

    fake_obs::RenameScene(a, "Renamed");
    Sync(model);
    QCOMPARE(Dump(model), QString("Renamed"));
  }

  void LoadSceneTreeRestoresFoldersOrderAndExpansion() {
    fake_obs::CreateScene("A");
    fake_obs::CreateScene("B");
    fake_obs::CreateScene("C");
    StvItemModel model;
    OBSDataAutoRelease saved =
        SavedTree({FolderEntry("F", {"B", "A"}, 0, true), SceneEntry("C")});
    model.LoadSceneTree(saved, kCollection);

    QCOMPARE(Dump(model), QString("F[B,A],C"));
    QVERIFY(model.item(0)->data(StvItemModel::kExpanded).toBool());
    QVERIFY(WeakOf(model.item(1)) != nullptr);
  }

  void SaveThenLoadRoundTrips() {
    fake_obs::CreateScene("A");
    fake_obs::CreateScene("B");
    fake_obs::CreateScene("C");
    StvItemModel model;
    OBSDataAutoRelease saved =
        SavedTree({FolderEntry("F", {"B", "A"}, 0, true), SceneEntry("C")});
    model.LoadSceneTree(saved, kCollection);

    OBSDataAutoRelease resaved = obs_data_create();
    model.SaveSceneTree(resaved, kCollection, nullptr);
    StvItemModel reloaded;
    reloaded.LoadSceneTree(resaved, kCollection);

    QCOMPARE(Dump(reloaded), QString("F[B,A],C"));
    QVERIFY(reloaded.item(0)->data(StvItemModel::kExpanded).toBool());
  }

  void LoadSceneTreeSkipsDuplicateSceneEntries() {
    fake_obs::CreateScene("A");
    StvItemModel model;
    OBSDataAutoRelease saved = SavedTree({SceneEntry("A"), SceneEntry("A")});
    model.LoadSceneTree(saved, kCollection);
    QCOMPARE(Dump(model), QString("A"));
  }

  // A saved name with no matching scene stays in the tree as a placeholder and
  // is filled in, keeping its position, when a scene with that name appears.
  void MissingSceneBecomesPlaceholderAndResolvesByName() {
    fake_obs::CreateScene("A");
    StvItemModel model;
    OBSDataAutoRelease saved =
        SavedTree({FolderEntry("F", {"Missing"}), SceneEntry("A")});
    model.LoadSceneTree(saved, kCollection);

    QStandardItem *placeholder = model.item(0)->child(0);
    QCOMPARE(Dump(model), QString("F[Missing],A"));
    QVERIFY(WeakOf(placeholder) == nullptr);

    fake_obs::CreateScene("Missing");
    Sync(model);
    QCOMPARE(Dump(model), QString("F[Missing],A"));
    QVERIFY(WeakOf(placeholder) != nullptr);
  }

  void MoveIndexByOneMovesSceneDownAndUp() {
    fake_obs::CreateScene("A");
    fake_obs::CreateScene("B");
    fake_obs::CreateScene("C");
    StvItemModel model;
    OBSDataAutoRelease saved =
        SavedTree({SceneEntry("A"), SceneEntry("B"), SceneEntry("C")});
    model.LoadSceneTree(saved, kCollection);

    QVERIFY(model.MoveIndexByOne(model.index(0, 0), +1));
    QCOMPARE(Dump(model), QString("B,A,C"));
    QVERIFY(model.MoveIndexByOne(model.index(2, 0), -1));
    QCOMPARE(Dump(model), QString("B,C,A"));
  }

  void MoveIndexByOneRejectsMovesPastTheEnds() {
    fake_obs::CreateScene("A");
    fake_obs::CreateScene("B");
    StvItemModel model;
    OBSDataAutoRelease saved = SavedTree({SceneEntry("A"), SceneEntry("B")});
    model.LoadSceneTree(saved, kCollection);

    QVERIFY(!model.MoveIndexByOne(model.index(0, 0), -1));
    QVERIFY(!model.MoveIndexByOne(model.index(1, 0), +1));
    QCOMPARE(Dump(model), QString("A,B"));
  }

  void MoveIndexByOneMovesFolderWithItsChildren() {
    fake_obs::CreateScene("A");
    fake_obs::CreateScene("B");
    fake_obs::CreateScene("C");
    StvItemModel model;
    OBSDataAutoRelease saved =
        SavedTree({FolderEntry("F", {"A", "B"}), SceneEntry("C")});
    model.LoadSceneTree(saved, kCollection);

    QVERIFY(model.MoveIndexByOne(model.index(0, 0), +1));
    QCOMPARE(Dump(model), QString("C,F[A,B]"));
  }

  // After a move the model must still track the scene's new item.
  void MovedSceneIsStillFoundAsCurrentScene() {
    obs_source_t *a = fake_obs::CreateScene("A");
    fake_obs::CreateScene("B");
    StvItemModel model;
    OBSDataAutoRelease saved = SavedTree({SceneEntry("A"), SceneEntry("B")});
    model.LoadSceneTree(saved, kCollection);
    QVERIFY(model.MoveIndexByOne(model.index(0, 0), +1));

    obs_frontend_set_current_scene(a);
    QCOMPARE(model.GetCurrentSceneItem(), model.item(1));
  }

  void SetSelectedSceneSwitchesObsScene() {
    obs_source_t *a = fake_obs::CreateScene("A");
    StvItemModel model;
    Sync(model);

    model.SetSelectedScene(model.item(0), /*set_preview_scene=*/false);
    QCOMPARE(fake_obs::CurrentScene(), a);
  }

  void SortFolderSortsAscendingAndDescending() {
    fake_obs::CreateScene("b");
    fake_obs::CreateScene("a");
    fake_obs::CreateScene("c");
    StvItemModel model;
    OBSDataAutoRelease saved = SavedTree({FolderEntry(
        "F", {"b", "a", "c"},
        static_cast<long long>(StvItemModel::SortMode::kAlphaAsc))});
    model.LoadSceneTree(saved, kCollection);
    QCOMPARE(Dump(model), QString("F[a,b,c]"));

    QStandardItem *folder = model.item(0);
    folder->setData(static_cast<int>(StvItemModel::SortMode::kAlphaDesc),
                    StvItemModel::kSortMode);
    model.SortFolder(folder);
    QCOMPARE(Dump(model), QString("F[c,b,a]"));
  }

  // A sorted folder remembers its manual order, restores it on request, and
  // saves that manual order rather than the displayed one.
  void SortedFolderKeepsAndSavesManualOrder() {
    fake_obs::CreateScene("b");
    fake_obs::CreateScene("a");
    fake_obs::CreateScene("c");
    StvItemModel model;
    OBSDataAutoRelease saved = SavedTree({FolderEntry(
        "F", {"b", "a", "c"},
        static_cast<long long>(StvItemModel::SortMode::kAlphaAsc))});
    model.LoadSceneTree(saved, kCollection);

    OBSDataAutoRelease resaved = obs_data_create();
    model.SaveSceneTree(resaved, kCollection, nullptr);
    OBSDataArrayAutoRelease root = obs_data_get_array(resaved, kCollection);
    OBSDataAutoRelease folder_entry = obs_data_array_item(root, 0);
    OBSDataArrayAutoRelease children =
        obs_data_get_array(folder_entry, "folder");
    OBSDataAutoRelease first = obs_data_array_item(children, 0);
    QCOMPARE(QString(obs_data_get_string(first, "name")), QString("b"));

    model.RestoreUserOrder(model.item(0));
    QCOMPARE(Dump(model), QString("F[b,a,c]"));
  }

  void CreateUniqueFolderNameNumbersDuplicates() {
    StvItemModel model;
    QStandardItem *root = model.invisibleRootItem();
    auto *first = new StvFolderItem("Folder");
    auto *second = new StvFolderItem("Folder");
    root->appendRow(first);
    root->appendRow(second);

    QCOMPARE(model.CreateUniqueFolderName(first, root), QString("Folder 1"));
    second->setText("Other");
    QCOMPARE(model.CreateUniqueFolderName(first, root), QString("Folder"));
  }

  // The model moves the dragged items itself and returns false so the view
  // does not also remove the source rows. Known quirk: dropping onto a folder
  // inserts at its top.
  void DropMimeDataMovesSceneIntoFolder() {
    fake_obs::CreateScene("A");
    fake_obs::CreateScene("B");
    StvItemModel model;
    OBSDataAutoRelease saved =
        SavedTree({FolderEntry("F", {"A"}), SceneEntry("B")});
    model.LoadSceneTree(saved, kCollection);

    std::unique_ptr<QMimeData> mime(model.mimeData({model.index(1, 0)}));
    QVERIFY(!model.dropMimeData(mime.get(), Qt::MoveAction, -1, 0,
                                model.index(0, 0)));
    QCOMPARE(Dump(model), QString("F[B,A]"));
  }

  void DropMimeDataReordersWithinTheSameParent() {
    fake_obs::CreateScene("A");
    fake_obs::CreateScene("B");
    fake_obs::CreateScene("C");
    StvItemModel model;
    OBSDataAutoRelease saved =
        SavedTree({SceneEntry("A"), SceneEntry("B"), SceneEntry("C")});
    model.LoadSceneTree(saved, kCollection);

    std::unique_ptr<QMimeData> down(model.mimeData({model.index(0, 0)}));
    model.dropMimeData(down.get(), Qt::MoveAction, 3, 0, QModelIndex());
    QCOMPARE(Dump(model), QString("B,C,A"));

    std::unique_ptr<QMimeData> up(model.mimeData({model.index(2, 0)}));
    model.dropMimeData(up.get(), Qt::MoveAction, 1, 0, QModelIndex());
    QCOMPARE(Dump(model), QString("B,A,C"));
  }

  // Dragging a folder together with a scene inside it moves the folder once
  // and leaves the scene where it is within the folder.
  void DropMimeDataDoesNotDuplicateSceneDraggedWithItsFolder() {
    fake_obs::CreateScene("A");
    fake_obs::CreateScene("B");
    StvItemModel model;
    OBSDataAutoRelease saved =
        SavedTree({FolderEntry("F", {"A"}), FolderEntry("G", {"B"})});
    model.LoadSceneTree(saved, kCollection);

    const QModelIndex folder = model.index(0, 0);
    std::unique_ptr<QMimeData> mime(
        model.mimeData({folder, model.index(0, 0, folder)}));
    model.dropMimeData(mime.get(), Qt::MoveAction, 0, 0, model.index(1, 0));
    QCOMPARE(Dump(model), QString("G[F[A],B]"));
  }

  void DropMimeDataRenamesFolderThatClashesInItsNewParent() {
    StvItemModel model;
    OBSDataAutoRelease saved =
        SavedTree({FolderEntry("F", {}), FolderEntry("G", {})});
    model.LoadSceneTree(saved, kCollection);
    auto *inner = new StvFolderItem("F");
    model.item(1)->appendRow(inner);

    std::unique_ptr<QMimeData> mime(model.mimeData({inner->index()}));
    model.dropMimeData(mime.get(), Qt::MoveAction, 0, 0, QModelIndex());
    QCOMPARE(Dump(model), QString("F 1[],F[],G[]"));
  }

  void DropMimeDataIgnoresDropOfFolderIntoItself() {
    StvItemModel model;
    OBSDataAutoRelease saved = SavedTree({FolderEntry("F", {})});
    model.LoadSceneTree(saved, kCollection);

    std::unique_ptr<QMimeData> mime(model.mimeData({model.index(0, 0)}));
    model.dropMimeData(mime.get(), Qt::MoveAction, 0, 0, model.index(0, 0));
    QCOMPARE(Dump(model), QString("F[]"));
  }

  // A placeholder has no scene behind it; moving it must move only it.
  void MoveIndexByOneMovesPlaceholderWithoutTouchingSiblings() {
    fake_obs::CreateScene("A");
    fake_obs::CreateScene("B");
    StvItemModel model;
    OBSDataAutoRelease saved =
        SavedTree({SceneEntry("A"), SceneEntry("Missing"), SceneEntry("B")});
    model.LoadSceneTree(saved, kCollection);

    QVERIFY(model.MoveIndexByOne(model.index(1, 0), -1));
    QCOMPARE(Dump(model), QString("Missing,A,B"));
    QVERIFY(model.MoveIndexByOne(model.index(0, 0), +1));
    QCOMPARE(Dump(model), QString("A,Missing,B"));

    Sync(model);
    QCOMPARE(Dump(model), QString("A,Missing,B"));
  }

  void MovedFolderKeepsPlaceholderChildrenAndExpansion() {
    fake_obs::CreateScene("A");
    fake_obs::CreateScene("B");
    fake_obs::CreateScene("C");
    StvItemModel model;
    OBSDataAutoRelease saved = SavedTree(
        {FolderEntry("F", {"A", "Missing", "B"}, 0, true), SceneEntry("C")});
    model.LoadSceneTree(saved, kCollection);

    QVERIFY(model.MoveIndexByOne(model.index(0, 0), +1));
    QCOMPARE(Dump(model), QString("C,F[A,Missing,B]"));
    QVERIFY(model.item(1)->data(StvItemModel::kExpanded).toBool());
  }

  void MoveKeepsTheSameItemObject() {
    fake_obs::CreateScene("A");
    fake_obs::CreateScene("B");
    StvItemModel model;
    OBSDataAutoRelease saved = SavedTree({SceneEntry("A"), SceneEntry("B")});
    model.LoadSceneTree(saved, kCollection);

    QStandardItem *a = model.item(0);
    QVERIFY(model.MoveIndexByOne(model.index(0, 0), +1));
    QCOMPARE(model.item(1), a);
  }
};

}  // namespace
}  // namespace scene_tree_view

QTEST_MAIN(scene_tree_view::StvItemModelTest)
#include "stv_item_model_test.moc"
