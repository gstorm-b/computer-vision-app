#include <vtkCollisionDetectionFilter.h>
#include <vtkCubeSource.h>
#include <vtkMatrix4x4.h>
#include <vtkNew.h>
#include <vtkTriangleFilter.h>

#include <iostream>

/// Local helper for the mesh collision VTK debug spike, used to build the paired cube
/// geometries and count collision contacts between them at a given offset.
namespace {

/// Builds two axis-aligned 1x1x1 unit cubes (triangulated via vtkTriangleFilter), leaves
/// meshA at the origin and translates meshB by `offsetX` along X, then runs
/// vtkCollisionDetectionFilter in "all contacts" mode between them.
/// @param offsetX translation applied to meshB along X, controlling whether the cubes overlap
/// @return the number of contacts vtkCollisionDetectionFilter reports between the two cubes
int contactCountForOffset(const double offsetX)
{
    vtkNew<vtkCubeSource> meshA;
    meshA->SetXLength(1.0);
    meshA->SetYLength(1.0);
    meshA->SetZLength(1.0);

    vtkNew<vtkCubeSource> meshB;
    meshB->SetXLength(1.0);
    meshB->SetYLength(1.0);
    meshB->SetZLength(1.0);

    vtkNew<vtkTriangleFilter> trianglesA;
    trianglesA->SetInputConnection(meshA->GetOutputPort());
    trianglesA->Update();

    vtkNew<vtkTriangleFilter> trianglesB;
    trianglesB->SetInputConnection(meshB->GetOutputPort());
    trianglesB->Update();

    vtkNew<vtkMatrix4x4> matrixA;
    matrixA->Identity();

    vtkNew<vtkMatrix4x4> matrixB;
    matrixB->Identity();
    matrixB->SetElement(0, 3, offsetX);

    vtkNew<vtkCollisionDetectionFilter> collision;
    collision->SetInputData(0, trianglesA->GetOutput());
    collision->SetInputData(1, trianglesB->GetOutput());
    collision->SetMatrix(0, matrixA);
    collision->SetMatrix(1, matrixB);
    collision->SetBoxTolerance(0.0f);
    collision->SetCellTolerance(0.0);
    collision->SetCollisionModeToAllContacts();
    collision->GenerateScalarsOff();
    collision->Update();

    return collision->GetNumberOfContacts();
}

}

/// Entry point for the VTK mesh-collision debug spike: checks contactCountForOffset()
/// reports at least one contact for overlapping cubes (offset 0.25) and exactly zero
/// contacts for separated cubes (offset 3.0), printing the counts and a pass/fail message.
/// @return 0 if both checks pass; 1 if either check fails
int main()
{
    const int collidingContacts = contactCountForOffset(0.25);
    const int separatedContacts = contactCountForOffset(3.0);

    std::cout << "vtk_debug colliding_contacts=" << collidingContacts << std::endl;
    std::cout << "vtk_debug separated_contacts=" << separatedContacts << std::endl;

    if (collidingContacts <= 0) {
        std::cerr << "[ERROR] Expected a colliding query to report contacts." << std::endl;
        return 1;
    }
    if (separatedContacts != 0) {
        std::cerr << "[ERROR] Expected a separated query to report zero contacts." << std::endl;
        return 1;
    }

    std::cout << "[OK] VTK debug spike query passed." << std::endl;
    return 0;
}
