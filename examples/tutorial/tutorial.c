
/*
  A pressure-loaded plate example for TACS.

  Copyright (c) 2010-2016 Graeme Kennedy. All rights reserved. 
*/

#include "TACSAssembler.h"
#include "MITCShell.h"
#include "TACSShellTraction.h"
#include "isoFSDTStiffness.h"
#include "TACSToFH5.h"

/*
  The following example demonstrates the use of TACS on a pressure
  loaded plate. 

  This code uses the TACSAssembler interface directly. Other creation
  code (TACSCreator/TACSMeshLoader) can also be used to generate
  TACSAssembler instances. Once a TACSAssembler instance has been
  created and initialized, it should be able to be used interchangeably
  withe 
  
  Note: This code does not intelligently partition the mesh. You could
  use TACSCreator to perform the partitioning for you to achieve
  better results, but this code is designed to demonstrate the
  TACSAssembler interface itself.

  The command line inputs (nx, ny) provide the number of elements
  along the x and y directions, respectively.
*/
int main( int argc, char * argv[] ){
  MPI_Init(&argc, &argv);
  
  // Find the MPI rank and size
  MPI_Comm tacs_comm = MPI_COMM_WORLD;
  int rank, size;
  MPI_Comm_rank(tacs_comm, &rank);
  MPI_Comm_size(tacs_comm, &size);

  // Set the dimensions of the plate
  TacsScalar Lx = 1.0;
  TacsScalar Ly = 1.0;

  // Get the global size of the mesh from the input
  int nx = 30, ny = 30;
  for ( int k = 0; k < argc; k++ ){
    int _nx, _ny;
    if (sscanf(argv[k], "nx=%d", &_nx) == 1){
      nx = (_nx > 2 ? _nx : 2);
    }
    if (sscanf(argv[k], "ny=%d", &_ny) == 1){
      ny = (_ny > 2 ? _ny : 2);
    }
  }

  /*
    To create TACS we need the following information:

    1. The communicator
    2. The number of variables per node (the same across the entire
    mesh)
    3. The number of nodes that are owned by this processor
    4. The number of elements that are owned by this processor
    5. The number of dependent nodes (nodes that depend linearly
    on other nodes)

    In this example, nx and ny are the number of elements in the
    global element mesh. Note that TACS deals exclusively with global
    node numbers to try to make things easier for the user.

    The ownership range of each processor (the range of node numbers
    that belong to each processor) is calculated using
  */

  // We know in advance that the number of unknowns per node is
  // going to be equal to 6 (You can find this value by checking
  // with element->numDisplacements() which returns the number
  // of displacements (or unknowns) per node)
  int varsPerNode = 6;  

  int nodesPerProc = ((nx+1)*(ny+1))/size;
  int elemsPerProc = (nx*ny)/size;

  int numOwnedNodes = nodesPerProc;
  int numElements = elemsPerProc;

  // On the ast rank, adjust the ownership so we get the
  // total that we need
  if (rank == size-1){
    numOwnedNodes = (nx+1)*(ny+1) - nodesPerProc*(size-1);
    numElements = nx*ny - elemsPerProc*(size-1);
  }

  // There are no dependent nodes in this problem
  int numDependentNodes = 0;
  TACSAssembler * tacs = new TACSAssembler(tacs_comm, varsPerNode,
					   numOwnedNodes, numElements,
                                           numDependentNodes);
  tacs->incref(); // Increase the reference count to TACSAssembler

  // Set the global element index for the first and last element 
  // in the partition
  int firstElem = rank*elemsPerProc;
  int firstNode = rank*nodesPerProc;

  int lastElem = (rank+1)*elemsPerProc;
  int lastNode = (rank+1)*nodesPerProc;
  if (rank == size-1){
    lastElem = nx*ny;
    lastNode = (nx+1)*(ny+1);
  }

  /*
    The element connectivity defines the mapping between the element
    and its corresponding nodes. The node numbers are global. Since
    the number of nodes per element may vary, we also provide a
    pointer into the element connectivity array denoting the begining
    location of each element node list. This data is passed in to
    TACSAssembler directly.

    In this case we know that we only ever have 4 nodes per element.
  */

  // The elements are ordered as (i + j*nx)
  int *ptr = new int[ numElements+1 ];
  int *conn = new int[ 4*numElements ];

  ptr[0] = 0;
  for ( int k = 0, elem = firstElem; elem < lastElem; k++, elem++ ){
    // Back out the i, j coordinates from the corresponding
    // element number
    int i = elem % nx;
    int j = elem/nx;

    // Set the node connectivity
    conn[4*k] = i + j*(nx+1);
    conn[4*k+1] = i+1 + j*(nx+1);
    conn[4*k+2] = i + (j+1)*(nx+1);
    conn[4*k+3] = i+1 + (j+1)*(nx+1);
    ptr[k+1] = 4*(k+1);
  }
  
  // Set the connectivity
  tacs->setElementConnectivity(conn, ptr);
  delete [] conn;
  delete [] ptr;

  // Create and set the elements
  TACSElement **elements = new TACSElement*[ numElements ];

  // Create the auxiliary element class - we'll use this to apply
  // surface tractions
  TACSAuxElements *aux = new TACSAuxElements(numElements);
  
  for ( int k = 0, elem = firstElem; elem < lastElem; k++, elem++ ){
    // Create the constitutive objects
    TacsScalar rho = 2500.0; // Not used
    TacsScalar E = 70e9;
    TacsScalar nu = 0.3;
    TacsScalar kcorr = 5.0/6.0; // The shear correction factor
    TacsScalar yield_stress = 464.0e6;
    TacsScalar thickness = 0.005;

    // Set the thickness design variable = the element number
    int tNum = elem;
    
    // Create the stiffness object
    isoFSDTStiffness *stiff = new isoFSDTStiffness(rho, E, nu, kcorr, 
                                                   yield_stress, thickness, 
                                                   tNum);

    // Create the shell element    
    elements[k] = new MITCShell<2>(stiff);

    // Create a surface traction associated with this element and add
    // it to the auxilary elements. Note that the element number must
    // correspond to the local element number used for this processor.
    TacsScalar tx = 0.0, ty = 0.0, tz = -1e5; 
    TACSShellTraction<2> *trac = new TACSShellTraction<2>(tx, ty, tz);
    aux->addElement(k, trac);
  }

  // Set the elements into the mesh
  tacs->setElements(elements);

  // Set the boundary conditions - this will only record the
  // boundary conditions on its own nodes
  for ( int i = 0; i < nx+1; i++ ){
    int nodes[] = {i, i + (nx+1)*ny, i*(nx+1), (i+1)*(nx+1)-1};
    tacs->addBCs(4, nodes);
  }

  // Reorder the nodal variables
  int use_fe_mat = 0;
  int reorder = 0;
  enum TACSAssembler::OrderingType order_type = TACSAssembler::ND_ORDER;
  enum TACSAssembler::MatrixOrderingType mat_type = 
    TACSAssembler::APPROXIMATE_SCHUR;

  for ( int k = 0; k < argc; k++ ){
    if (strcmp(argv[k], "AMD") == 0){ 
      order_type = TACSAssembler::AMD_ORDER; reorder = 1;
    }
    else if (strcmp(argv[k], "RCM") == 0){ 
      order_type = TACSAssembler::RCM_ORDER; reorder = 1;
    }
    else if (strcmp(argv[k], "ND") == 0){ 
      order_type = TACSAssembler::ND_ORDER; reorder = 1;
    }
    else if (strcmp(argv[k], "DirectSchur") == 0){ 
      mat_type = TACSAssembler::DIRECT_SCHUR; reorder = 1;
    }
    else if (strcmp(argv[k], "ApproximateSchur") == 0){ 
      mat_type = TACSAssembler::APPROXIMATE_SCHUR; reorder = 1;
    }
    else if (strcmp(argv[k], "AdditiveSchwarz") == 0){ 
      mat_type = TACSAssembler::ADDITIVE_SCHWARZ; reorder = 1;
    }
    else if (strcmp(argv[k], "DirectSolve") == 0){
      use_fe_mat = 1;
    }
  }

  // Reorder the nodal variables
  if (reorder){
    tacs->computeReordering(order_type, mat_type);
  }

  // Perform initialization - cannot add any more elements/vars etc
  tacs->initialize();

  // Create the node vector
  TACSBVec *X = tacs->createNodeVec();
  X->incref();

  // Get the local node locations
  TacsScalar *Xpts = NULL;
  X->getArray(&Xpts);
  for ( int k = 0, node = firstNode; node < lastNode; k += 3, node++ ){
    int i = node % (nx+1);
    int j = node/(nx+1);
    Xpts[k] = Lx*i/nx;
    Xpts[k+1] = Ly*j/ny;
  }

  // Reorder the vector if required
  if (reorder){
    tacs->reorderVec(X);
  }

  // Set the node locations
  tacs->setNodes(X);

  // Set the auxiliary elements
  tacs->setAuxElements(aux);

  // Solve the problem and set the variables into TACS
  TACSMat *kmat = NULL;
  TACSMat *mmat = NULL;
  TACSPc *pc = NULL;

  // Depending on the input options, solve the 
  int lev_fill = 5; // ILU(k) fill in
  int fill = 8.0; // Expected number of non-zero entries

  // Options for the ApproximateSchur preconditioner class
  int inner_gmres_iters = 10; 
  double inner_rtol = 1e-4, inner_atol = 1e-30;

  // These calls compute the symbolic factorization and allocate
  // the space required for the preconditioners
  if (use_fe_mat){
    FEMat *_kmat = tacs->createFEMat(order_type);
    FEMat *_mmat = tacs->createFEMat();
    int reorder_schur = 1;
    pc = new PcScMat(_kmat, lev_fill, fill, reorder_schur);
    kmat = _kmat;
    mmat = _mmat;
  }
  else {
    // Adjust the level of fill based on the input argument
    for ( int k = 0; k < argc; k++ ){
      int _lev_fill;
      if (sscanf(argv[k], "lev_fill=%d", &_lev_fill) == 1){
        lev_fill = _lev_fill;
      }
    }

    // Create the distributed matrix class
    DistMat *_kmat = tacs->createMat();
    DistMat *_mmat = tacs->createMat();
    pc = new ApproximateSchur(_kmat, lev_fill, fill, 
			      inner_gmres_iters, inner_rtol, inner_atol);
    kmat = _kmat;
    mmat = _mmat;
  }
  mmat->incref();
  kmat->incref();
  pc->incref();

  // Assemble the stiffness matrix and residual
  TACSBVec *res = tacs->createVec();  res->incref();
  TACSBVec *ans = tacs->createVec();  ans->incref();
  TACSBVec *tmp = tacs->createVec();  tmp->incref();

  // Assemble the Jacobian of governing equations
  double alpha = 1.0, beta = 0.0, gamma = 0.0;
  tacs->assembleJacobian(res, kmat, alpha, beta, gamma);

  // This call copies then factors the matrix
  double t0 = MPI_Wtime();
  pc->factor(); 
  t0 = MPI_Wtime() - t0;
  printf("[%d] Factor time %f \n", rank, t0);

  // Now, set up the solver
  int use_gmres = 1;
  int gmres_iters = 15; 
  int nrestart = 2; // Number of allowed restarts
  int is_flexible = 1; // Is a flexible preconditioner?

  // Arguments for the ApproximateSchur preconditioner
  int outer_iters = 15; // Outer subspace size
  int max_outer_iters = 45; // Maximum number of outer iterations

  // Create the Krylov Subspace Method (KSM) object
  TACSKsm *ksm = NULL;
  int freq = 1;
  if (use_gmres){
    ksm = new GMRES(kmat, pc, gmres_iters, nrestart, is_flexible);
    ksm->setMonitor(new KSMPrintStdout("GMRES", rank, freq));
  }
  else {
    ksm = new GCROT(kmat, pc, outer_iters, max_outer_iters,
		    gmres_iters, is_flexible);
    ksm->setMonitor(new KSMPrintStdout("GCROT", rank, freq));
  }
  ksm->incref();  

  // Test the actual residual
  ksm->solve(res, ans);
  kmat->mult(ans, tmp);
  tmp->axpy(-1.0, res);
  TacsScalar norm = tmp->norm();
  if (rank == 0){
    printf("|Ax - b|: %15.5e\n", RealPart(norm));
  }

  // Assemble the residual and print the result
  ans->scale(-1.0);
  tacs->setVariables(ans);
  tacs->assembleRes(res);
  norm = res->norm();  
  if (rank == 0){
    printf("|R|: %15.5e\n", RealPart(norm));
  }

  /* tacs->assembleJacobian(res, kmat, alpha, beta, gamma); */
  /* if (rank == 0){ */
  /*   printf("|R|: %15.5e\n", RealPart(norm)); */
  /* } */

  tacs->setVariables(res);

  // Output for visualization
  unsigned int write_flag = (TACSElement::OUTPUT_NODES |
                             TACSElement::OUTPUT_DISPLACEMENTS |
                             TACSElement::OUTPUT_STRAINS |
                             TACSElement::OUTPUT_STRESSES |
                             TACSElement::OUTPUT_EXTRAS);
  TACSToFH5 *f5 = new TACSToFH5(tacs, SHELL, write_flag);
  f5->incref();
  f5->writeToFile("tutorial_output.f5");
  f5->decref();

  /*
  // Now calculate the total derivate
  TacsScalar * dfdx = new TacsScalar[ num_design_vars ];

  // Compliance * func = new Compliance(tacs);
  PNormFailure * func = new PNormFailure(tacs, 10.0);
  func->incref();

  // Evaluate the func
  TacsScalar comp0 = tacs->evalFunction(loadCase, func);

  TacsScalar * dvs = new TacsScalar[ num_design_vars ];
  tacs->getDesignVars(dvs, num_design_vars); 
  tacs->setDesignVars(dvs, num_design_vars);

  // Re-evaluate the problem   
  tacs->zeroVariables(loadCase);
  tacs->assembleMat(loadCase, kmat, res);
  pc->factor(); 

  ksm->solve(res, ans);
  ans->scale(-1.0);
  tacs->setVariables(loadCase, ans);  

  comp0 = tacs->evalFunction(loadCase, func);
  if (rank == 0){
    printf("The %s is %15.8f \n", func->functionName(), RealPart(comp0));
  }

  tacs->evalSVSens(loadCase, func, res);
  ksm->solve(res, ans);

  // Evaluate the product of the adjoint variables with the derivative of the
  // residuals w.r.t. the design variables
  TacsScalar * adjResProduct = new TacsScalar[ num_design_vars ];
  tacs->evalAdjointResProduct(loadCase, ans, 
                              adjResProduct, num_design_vars);

  tacs->evalDVSens(loadCase, func, dfdx, num_design_vars);

  for ( int k = 0; k < num_design_vars; k++ ){
    dfdx[k] -= adjResProduct[k];
  }

  // Now check with a finite-difference projected derivative
  double dh = 0.5e-5;

  TacsScalar proj_deriv = 0.0;
  for ( int k = 0; k < num_design_vars; k++ ){
    double pert = sin((k*M_PI)/(num_design_vars-1));

    dvs[k] += pert*dh;
    proj_deriv += pert*dfdx[k];    
  }

  tacs->setDesignVars(dvs, num_design_vars);

  // Re-evaluate the problem   
  tacs->zeroVariables(loadCase);
  tacs->assembleMat(loadCase, kmat, res);
  pc->factor(); 

  ksm->solve(res, ans);
  ans->scale(-1.0);
  tacs->setVariables(loadCase, ans);

  // Evaluate the func
  TacsScalar comp1 = tacs->evalFunction(loadCase, func);
  if (rank == 0){
    TacsScalar fd = (comp1 - comp0)/dh;
    printf("The %s is %15.8f \n", func->functionName(), RealPart(comp1));
    printf("The projected derivative is             %20.8e \n", 
           RealPart(proj_deriv));
    printf("The finite-difference approximation is  %20.8e \n", 
           RealPart(fd));
    printf("The relative error is                   %20.5e \n", 
	   fabs(RealPart((fd - proj_deriv)/fd)));
  } 

  delete [] dvs;
  delete [] dfdx;
  delete [] adjResProduct;

  func->decref();
  */

  ksm->decref();
  pc->decref();
  kmat->decref();
  mmat->decref();
  ans->decref();
  res->decref();
  tmp->decref();
  tacs->decref();

  MPI_Finalize();

  return (0);
}
