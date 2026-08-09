#ifndef RALLYSIMULATOR_OBJ_LOADER_H
#define RALLYSIMULATOR_OBJ_LOADER_H

#ifdef __cplusplus
extern "C" {
#endif

typedef struct ObjVertex
{
    float x;
    float y;
    float z;
} ObjVertex;

typedef struct ObjMesh
{
    ObjVertex *vertices;
    unsigned int vertexCount;
} ObjMesh;

/* Loads triangulated OBJ faces. Vertex, texture and normal indices are
 * accepted (for example: f 1/2/3 4/5/6 7/8/9); only positions are used. */
int objLoad(ObjMesh *mesh, const char *path);
void objFree(ObjMesh *mesh);

#ifdef __cplusplus
}
#endif

#endif
