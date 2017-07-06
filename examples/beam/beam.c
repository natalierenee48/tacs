#include "TimoshenkoStiffness.h"
#include "MITC3.h"
#include "TACSAssembler.h"
#include "RigidBody.h"
#include "TACSIntegrator.h"


/*
  Create and return the TACSAssembler object for the four bar
  mechanism as described by Bachau

  B ------------------- C
  |                     |
  |                     |
  |                     |
  A                     D

  Length between A and B = 0.12 m
  Length between B and C = 0.24 m
  Length between C and D = 0.12 m

  A, B and D are revolute joints in the plane perpendicular to the
  plane of the mechanism
  
  C is a revolute joint in a plane +5 degrees along the DC axis of the
  beam

  Beam properties:
  
  Young's modulus 207 GPa, nu = 0.3

  Bars 1 and 2 are square and of dimension 16 x 16 mm
  Bar 3 is square and of dimension 8 x 8 mm
*/
TACSAssembler *four_bar_mechanism(){
  /*
  // Set the gravity vector
  TACSGibbsVector *gravity = new TACSGibbsVector(0.0, 0.0, -9.81);

  // Create the revolute direction for A, B and D
  TACSGibbsVector *revA = new TACSGibbsVector(0.0, 0.0, 1.0);

  // Create the revolute direction for C
  TacsScalar theta = (5.0/180.0)*M_PI;
  TACSGibbsVector *revC = new TACSGibbsVector(sin(theta), 0.0, cos(theta));
  
  
  
  

  */

}




int main( int argc, char *argv[] ){
  // Initialize MPI
  MPI_Init(&argc, &argv);

  // Set the reference axis
  TacsScalar axis[] = {0.0, 1.0, 0.0};

  // Set the gravity vector
  TACSGibbsVector *gravity = new TACSGibbsVector(0.0, 0.0, -9.81);
  
  // Set the element properties  
  TacsScalar rhoA = 1.5;
  TacsScalar rhoIy = 0.15;
  TacsScalar rhoIz = 0.15;
  TacsScalar rhoIyz = 0.0;

  TacsScalar EA = 1e4;
  TacsScalar GJ = 1.50e4;
  TacsScalar EIy = 2.4e4;
  TacsScalar EIz = 3.24e4;
  TacsScalar kGAy = 2.5e3;
  TacsScalar kGAz = 5.2e3;
  
  // Create the Timoshenko stiffness object
  TimoshenkoStiffness *stiff =
    new TimoshenkoStiffness(rhoA, rhoIy, rhoIz, rhoIyz,
                            EA, GJ, EIy, EIz, kGAy, kGAz,
                            axis);
  stiff->incref();

  // Create the MITC3 element
  MITC3 *beam = new MITC3(stiff, gravity);
  beam->incref();

  int test_element = 1;
  if (test_element){
    TacsScalar X[] = {0.0, 0.0, 0.0,
                      0.375, 0.375, 0.1,
                      1.0, 1.0, 0.2};
    beam->testStrain(X);
    
    TacsScalar vars[24], dvars[24], ddvars[24];
    for ( int i = 0; i < 24; i++ ){
      vars[i] = -1.0 + 2.0*rand()/RAND_MAX;
      dvars[i] = -1.0 + 2.0*rand()/RAND_MAX;
      ddvars[i] = -1.0 + 2.0*rand()/RAND_MAX;
    }
    vars[7] = vars[15] = vars[23] = 0.0;
    
    beam->setStepSize(1e-5);
    beam->setPrintLevel(2);
    beam->testResidual(0.0, X, vars, dvars, ddvars);
    beam->testJacobian(0.0, X, vars, dvars, ddvars);
  }

  // Set the number of elements and nodes
  int nelems = 10;
  int nnodes = 2*nelems+1;

  // Set the locations for the beam
  MPI_Comm comm = MPI_COMM_WORLD;
  TACSAssembler *tacs = new TACSAssembler(comm, 8, 
                                          nnodes, nelems);
  tacs->incref();

  // Create the mesh
  TACSElement **elems = new TACSElement*[ nelems ];
  int *conn = new int[ 3*nelems ];
  int *ptr = new int[ nelems+1 ];

  // Create the connectivity and set the elements
  ptr[0] = 0;
  for ( int i = 0; i < nelems; i++ ){
    elems[i] = beam;
    conn[ptr[i]] = 2*i;
    conn[ptr[i]+1] = 2*i+1;
    conn[ptr[i]+2] = 2*i+2;
    ptr[i+1] = ptr[i] + 3;
  }

  tacs->setElementConnectivity(conn, ptr);
  delete [] conn;
  delete [] ptr;

  tacs->setElements(elems);
  delete [] elems;

  // Add boundary conditions
  int nodes = 0;
  int nbcs = 3;
  int vars[] = {0, 1, 2};
  tacs->addBCs(1, &nodes, nbcs, vars);

  tacs->initialize();

  // Set the node locations
  TACSBVec *Xvec = tacs->createNodeVec();
  Xvec->incref();
  TacsScalar *Xarray;
  Xvec->getArray(&Xarray);
  for ( int k = 0; k < 2*nelems+1; k++ ){
    Xarray[3*k] = 1.0*k/(2*nelems+1);
  }
  tacs->setNodes(Xvec);
  Xvec->decref();

  // Now... we're ready to simulate a falling beam
  int num_steps = 250;
  TACSIntegrator *integrator = 
    new TACSBDFIntegrator(tacs, 0.0, 2.0, num_steps, 2);
  integrator->incref();
  
  integrator->setAbsTol(1e-8);

  integrator->setOutputFrequency(1);
  integrator->setShellOutput(1);
  integrator->integrate();

  integrator->decref();
  tacs->decref();
  beam->decref();
  stiff->decref();

  MPI_Finalize();
  return 0;
}
