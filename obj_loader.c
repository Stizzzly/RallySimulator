#include "obj_loader.h"

#include <ctype.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

typedef struct ObjFace
{
    int index[3];
    unsigned int positionsAtFace;
} ObjFace;

static int growArray(void **data, unsigned int *capacity,
                     unsigned int required, size_t itemSize)
{
    unsigned int newCapacity;
    void *newData;

    if (required <= *capacity) return 1;
    newCapacity = *capacity == 0 ? 64 : *capacity * 2;
    while (newCapacity < required) newCapacity *= 2;
    newData = realloc(*data, itemSize * newCapacity);
    if (newData == NULL) return 0;
    *data = newData;
    *capacity = newCapacity;
    return 1;
}

static int parseIndex(const char *token)
{
    int index = 0;
    return sscanf(token, "%d", &index) == 1 ? index : 0;
}

static int resolveIndex(int rawIndex, unsigned int finalPositionCount,
                        unsigned int positionsAtFace)
{
    int index;

    if (rawIndex > 0) index = rawIndex - 1;
    else if (rawIndex < 0) index = (int)positionsAtFace + rawIndex;
    else return -1;

    if (index < 0 || (unsigned int)index >= finalPositionCount) return -1;
    return index;
}

void objFree(ObjMesh *mesh)
{
    if (mesh == NULL) return;
    free(mesh->vertices);
    mesh->vertices = NULL;
    mesh->vertexCount = 0;
}

int objLoad(ObjMesh *mesh, const char *path)
{
    FILE *file;
    char line[512];
    ObjVertex *positions = NULL;
    ObjFace *faces = NULL;
    unsigned int positionCount = 0;
    unsigned int positionCapacity = 0;
    unsigned int faceCount = 0;
    unsigned int faceCapacity = 0;
    unsigned int outputIndex = 0;
    int ok = 0;

    if (mesh == NULL || path == NULL) return 0;
    objFree(mesh);
    file = fopen(path, "r");
    if (file == NULL) return 0;

    /* fgets is reliable in PSL1GHT's newlib. sscanf is used only within one
     * already-complete line, so no parser state crosses a line boundary. */
    while (fgets(line, sizeof(line), file) != NULL) {
        char *text = line;
        while (*text != '\0' && isspace((unsigned char)*text)) ++text;

        if (text[0] == 'v' && isspace((unsigned char)text[1])) {
            ObjVertex vertex;
            if (sscanf(text + 1, "%f %f %f", &vertex.x, &vertex.y, &vertex.z) != 3 ||
                !growArray((void **)&positions, &positionCapacity,
                           positionCount + 1, sizeof(ObjVertex))) {
                goto cleanup;
            }
            positions[positionCount++] = vertex;
        } else if (text[0] == 'f' && isspace((unsigned char)text[1])) {
            char a[64];
            char b[64];
            char c[64];
            ObjFace face;

            if (sscanf(text + 1, "%63s %63s %63s", a, b, c) != 3 ||
                !growArray((void **)&faces, &faceCapacity,
                           faceCount + 1, sizeof(ObjFace))) {
                goto cleanup;
            }
            face.index[0] = parseIndex(a);
            face.index[1] = parseIndex(b);
            face.index[2] = parseIndex(c);
            face.positionsAtFace = positionCount;
            if (face.index[0] == 0 || face.index[1] == 0 || face.index[2] == 0) {
                goto cleanup;
            }
            faces[faceCount++] = face;
        }
    }

    if (positionCount == 0 || faceCount == 0) goto cleanup;
    mesh->vertices = (ObjVertex *)malloc(sizeof(ObjVertex) * faceCount * 3);
    if (mesh->vertices == NULL) goto cleanup;

    for (unsigned int i = 0; i < faceCount; ++i) {
        int a = resolveIndex(faces[i].index[0], positionCount, faces[i].positionsAtFace);
        int b = resolveIndex(faces[i].index[1], positionCount, faces[i].positionsAtFace);
        int c = resolveIndex(faces[i].index[2], positionCount, faces[i].positionsAtFace);
        if (a < 0 || b < 0 || c < 0) {
            objFree(mesh);
            goto cleanup;
        }
        mesh->vertices[outputIndex++] = positions[a];
        mesh->vertices[outputIndex++] = positions[b];
        mesh->vertices[outputIndex++] = positions[c];
    }

    mesh->vertexCount = outputIndex;
    ok = outputIndex != 0;

cleanup:
    fclose(file);
    free(positions);
    free(faces);
    return ok;
}
