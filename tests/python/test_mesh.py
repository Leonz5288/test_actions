import os

import numpy as np

import taichi as ti

this_dir = os.path.dirname(os.path.abspath(__file__))
model_file_path = os.path.join(this_dir, 'ell.json')


@ti.test(require=ti.extension.mesh, dynamic_index=False)
def test_mesh_patch_idx():
    mesh_builder = ti.Mesh.Tet()
    mesh_builder.verts.place({'idx': ti.i32})
    model = mesh_builder.build(ti.Mesh.load_meta(model_file_path))

    @ti.kernel
    def foo():
        for v in model.verts:
            v.idx = ti.mesh_patch_idx()

    foo()
    idx = model.verts.idx.to_numpy()
    assert idx[0] == 6
    assert idx.sum() == 89


def _test_mesh_for(cell_reorder=False, vert_reorder=False, extra_tests=True):
    mesh_builder = ti.Mesh.Tet()
    mesh_builder.verts.place({'t': ti.i32}, reorder=vert_reorder)
    mesh_builder.cells.place({'t': ti.i32}, reorder=cell_reorder)
    mesh_builder.cells.link(mesh_builder.verts)
    mesh_builder.verts.link(mesh_builder.cells)
    mesh_builder.cells.link(mesh_builder.cells)
    mesh_builder.verts.link(mesh_builder.verts)
    model = mesh_builder.build(ti.Mesh.load_meta(model_file_path))

    @ti.kernel
    def cell_vert():
        for c in model.cells:
            for j in range(c.verts.size):
                c.t += c.verts[j].id

    cell_vert()
    total = model.cells.t.to_numpy().sum()
    model.cells.t.fill(0)
    assert total == 892

    @ti.kernel
    def vert_cell():
        for v in model.verts:
            for j in range(v.cells.size):
                v.t += v.cells[j].id

    vert_cell()
    total = model.verts.t.to_numpy().sum()
    model.verts.t.fill(0)
    assert total == 1104

    if not extra_tests:
        return

    @ti.kernel
    def cell_cell():
        for c in model.cells:
            for j in range(c.cells.size):
                c.t += c.cells[j].id

    cell_cell()
    total = model.cells.t.to_numpy().sum()
    model.cells.t.fill(0)
    assert total == 690

    @ti.kernel
    def vert_vert():
        for v in model.verts:
            for j in range(v.verts.size):
                v.t += v.verts[j].id

    vert_vert()
    total = model.verts.t.to_numpy().sum()
    model.verts.t.fill(0)
    assert total == 1144


@ti.test(require=ti.extension.mesh, dynamic_index=False)
def test_mesh_for():
    _test_mesh_for(False, False)
    _test_mesh_for(False, True)


@ti.test(require=ti.extension.mesh,
         dynamic_index=False,
         optimize_mesh_reordered_mapping=False)
def test_mesh_reordered_opt():
    _test_mesh_for(True, True, False)


@ti.test(require=ti.extension.mesh,
         dynamic_index=False,
         mesh_localize_to_end_mapping=False)
def test_mesh_localize_mapping0():
    _test_mesh_for(False, False, False)
    _test_mesh_for(True, True, False)


@ti.test(require=ti.extension.mesh,
         dynamic_index=False,
         mesh_localize_from_end_mapping=True)
def test_mesh_localize_mapping1():
    _test_mesh_for(False, False, False)
    _test_mesh_for(True, True, False)


@ti.test(require=ti.extension.mesh, dynamic_index=False)
def test_mesh_reorder():
    vec3i = ti.types.vector(3, ti.i32)
    mesh_builder = ti.Mesh.Tet()
    mesh_builder.verts.place({'s': ti.i32, 's3': vec3i}, reorder=True)
    mesh_builder.cells.link(mesh_builder.verts)
    model = mesh_builder.build(ti.Mesh.load_meta(model_file_path))

    id2 = np.array([x**2 for x in range(len(model.verts))])
    id123 = np.array([[x**1, x**2, x**3] for x in range(len(model.verts))])
    model.verts.s.from_numpy(id2)
    model.verts.s3.from_numpy(id123)

    @ti.kernel
    def foo():
        for v in model.verts:
            assert v.s == v.id**2
            assert v.s3[0] == v.id**1 and v.s3[1] == v.id**2 and v.s3[
                2] == v.id**3
            v.s = v.id**3
            v.s3 *= v.id

    foo()

    id3 = model.verts.s.to_numpy()
    id234 = model.verts.s3.to_numpy()

    for i in range(len(model.verts)):
        assert model.verts.s[i] == i**3
        assert id3[i] == i**3
        assert model.verts.s3[i][0] == i**2
        assert model.verts.s3[i][1] == i**3
        assert model.verts.s3[i][2] == i**4
        assert id234[i][0] == i**2
        assert id234[i][1] == i**3
        assert id234[i][2] == i**4


@ti.test(require=ti.extension.mesh, dynamic_index=False)
def test_mesh_minor_relations():
    mesh_builder = ti.Mesh.Tet()
    mesh_builder.verts.place({'y': ti.i32})
    mesh_builder.edges.place({'x': ti.i32})
    mesh_builder.cells.link(mesh_builder.edges)
    mesh_builder.verts.link(mesh_builder.cells)
    model = mesh_builder.build(ti.Mesh.load_meta(model_file_path))
    model.edges.x.fill(1)

    @ti.kernel
    def foo():
        for v in model.verts:
            for i in range(v.cells.size):
                c = v.cells[i]
                for j in range(c.edges.size):
                    e = c.edges[j]
                    v.y += e.x

    foo()
    total = model.verts.y.to_numpy().sum()
    assert total == 576


@ti.test(require=ti.extension.mesh,
         dynamic_index=False,
         demote_no_access_mesh_fors=True)
def test_multiple_meshes():
    mesh_builder = ti.Mesh.Tet()
    mesh_builder.verts.place({'y': ti.i32})
    meta = ti.Mesh.load_meta(model_file_path)
    model1 = mesh_builder.build(meta)
    model2 = mesh_builder.build(meta)

    model1.verts.y.from_numpy(
        np.array([x**2 for x in range(len(model1.verts))]))

    @ti.kernel
    def foo():
        for v in model1.verts:
            model2.verts.y[v.id] = v.y

    foo()
    out = model2.verts.y.to_numpy()
    for i in range(len(out)):
        assert out[i] == i**2


@ti.test(require=ti.extension.mesh, dynamic_index=False)
def test_mesh_local():
    mesh_builder = ti.Mesh.Tet()
    mesh_builder.verts.place({'a': ti.i32})
    mesh_builder.faces.link(mesh_builder.verts)
    model = mesh_builder.build(ti.Mesh.load_meta(model_file_path))
    ext_a = ti.field(ti.i32, shape=len(model.verts))

    @ti.kernel
    def foo(cache: ti.template()):
        if ti.static(cache):
            ti.mesh_local(ext_a, model.verts.a)
        for f in model.faces:
            m = f.verts[0].id + f.verts[1].id + f.verts[2].id
            f.verts[0].a += m
            f.verts[1].a += m
            f.verts[2].a += m
            ext_a[f.verts[0].id] += m
            ext_a[f.verts[1].id] += m
            ext_a[f.verts[2].id] += m

    foo(False)
    res1 = model.verts.a.to_numpy()
    res2 = ext_a.to_numpy()
    model.verts.a.fill(0)
    ext_a.fill(0)
    foo(True)
    res3 = model.verts.a.to_numpy()
    res4 = ext_a.to_numpy()

    for i in range(len(model.verts)):
        assert res1[i] == res2[i]
        assert res1[i] == res3[i]
        assert res1[i] == res4[i]
