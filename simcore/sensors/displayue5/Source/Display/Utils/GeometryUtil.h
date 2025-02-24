#pragma once

#include "CustomMeshComponent.h"

namespace util
{
void ConeGeometry(
    TArray<FCustomMeshTriangle>& triangles, FVector Apex, float ConeRadius, float ConeHeight, int segments = 36)
{
    // Create the vertices for the base circle (lying on the YZ plane, but at X = height)
    std::vector<FVector> baseVertices;
    for (int i = 0; i < segments; ++i)
    {
        float angle = (2.0f * M_PI * i) / segments;           // Angle for each vertex
        float y = ConeRadius * cos(angle);                    // Y-coordinate of the base
        float z = ConeRadius * sin(angle);                    // Z-coordinate of the base
        baseVertices.push_back(FVector(ConeHeight, y, z));    // Base points at X = height
    }
    // Generate the cone's triangles (apex to base segments)
    for (int i = 0; i < segments; ++i)
    {
        int nextIndex = (i + 1) % segments;
        FCustomMeshTriangle tri;
        tri.Vertex0 = Apex;
        tri.Vertex1 = baseVertices[i];
        tri.Vertex2 = baseVertices[nextIndex];
        triangles.Add(tri);
    }
}

void SphereGeometry(TArray<FCustomMeshTriangle>& triangles, float Radius = 50.0f, float PhiStart = 0.0f,
    float PhiLength = 2 * PI, float ThetaStart = 0.0f, float ThetaLength = 2 * PI, int32 WidthSegments = 128,
    int32 HeightSegments = 128)
{
    // // Create the vertices for the base circle (lying on the YZ plane, but at X = height)
    // std::vector<FVector> baseVertices;
    // for (int i = 0; i < segments; ++i)
    // {
    //     float angle = (2.0f * M_PI * i) / segments;           // Angle for each vertex
    //     float y = ConeRadius * cos(angle);                    // Y-coordinate of the base
    //     float z = ConeRadius * sin(angle);                    // Z-coordinate of the base
    //     baseVertices.push_back(FVector(ConeHeight, y, z));    // Base points at X = height
    // }
    // // Generate the cone's triangles (apex to base segments)
    // for (int i = 0; i < segments; ++i)
    // {
    //     int nextIndex = (i + 1) % segments;
    //     FCustomMeshTriangle tri;
    //     tri.Vertex0 = Apex;
    //     tri.Vertex1 = baseVertices[i];
    //     tri.Vertex2 = baseVertices[nextIndex];
    //     triangles.Add(tri);
    // }

    TArray<FVector> Vertices;
    float PhiStep = PhiLength / WidthSegments;
    float ThetaStep = ThetaLength / HeightSegments;

    // Loop through each segment to generate the vertices and triangles
    for (int32 i = 0; i <= HeightSegments; ++i)
    {
        float Theta = ThetaStart + i * ThetaStep;
        float SinTheta = FMath::Sin(Theta);
        float CosTheta = FMath::Cos(Theta);

        for (int32 j = 0; j <= WidthSegments; ++j)
        {
            float Phi = PhiStart + j * PhiStep;
            float SinPhi = FMath::Sin(Phi);
            float CosPhi = FMath::Cos(Phi);

            // Calculate the vertex position (x, y, z) (y,z,x)
            float x = Radius * CosTheta;
            float y = Radius * SinTheta * CosPhi;
            float z = Radius * SinTheta * SinPhi;

            // Add the vertex to the vertices array
            Vertices.Add(FVector(x, y, z));
        }
    }

    // Now generate triangles from the vertex grid
    for (int32 i = 0; i < HeightSegments; ++i)
    {
        for (int32 j = 0; j < WidthSegments; ++j)
        {
            int32 First = (i * (WidthSegments + 1)) + j;
            int32 Second = First + WidthSegments + 1;
            int32 Third = First + 1;
            int32 Fourth = Second + 1;

            FCustomMeshTriangle tri1;
            tri1.Vertex0 = Vertices[First];
            tri1.Vertex1 = Vertices[Second];
            tri1.Vertex2 = Vertices[Third];
            triangles.Add(tri1);

            FCustomMeshTriangle tri2;
            tri2.Vertex0 = Vertices[Second];
            tri2.Vertex1 = Vertices[Fourth];
            tri2.Vertex2 = Vertices[Third];
            triangles.Add(tri2);
        }
    }
}

}    // namespace util