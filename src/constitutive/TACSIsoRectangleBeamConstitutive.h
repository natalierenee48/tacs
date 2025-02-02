/*
  This file is part of TACS: The Toolkit for the Analysis of Composite
  Structures, a parallel finite-element code for structural and
  multidisciplinary design optimization.

  Copyright (C) 2014 Georgia Tech Research Corporation

  TACS is licensed under the Apache License, Version 2.0 (the
  "License"); you may not use this software except in compliance with
  the License.  You may obtain a copy of the License at

  http://www.apache.org/licenses/LICENSE-2.0
*/

#ifndef TACS_ISO_RECTANGLE_BEAM_CONSTITUTIVE_H
#define TACS_ISO_RECTANGLE_BEAM_CONSTITUTIVE_H

/*
  This is a stiffness object for the Timoshenko beam class.
  These expressions represent a solid rectanglular cross-section.
  The assumed orientation of the cross-section is given in the figure below.
                                width
        <--------------------------------------------------->
        +---------------------------------------------------+    ^
        |                        ^  y_elem                  |    |
        |                        |                          |    |
        |                         ——> z_elem                |    | thickness
        |                                                   |    |
        |                                                   |    |
        +---------------------------------------------------+    v
*/

#include "TACSBeamConstitutive.h"
#include "TACSMaterialProperties.h"

class TACSIsoRectangleBeamConstitutive : public TACSBeamConstitutive {
 public:
  TACSIsoRectangleBeamConstitutive(TACSMaterialProperties *properties,
                                   TacsScalar _width, TacsScalar _thickness,
                                   int _width_num, int _thickness_num,
                                   TacsScalar _lb_width, TacsScalar _ub_width,
                                   TacsScalar _lb_thickness,
                                   TacsScalar _ub_thickness);
  ~TACSIsoRectangleBeamConstitutive();

  // Retrieve the global design variable numbers
  int getDesignVarNums(int elemIndex, int dvLen, int dvNums[]);

  // Set the element design variable from the design vector
  int setDesignVars(int elemIndex, int dvLen, const TacsScalar dvs[]);

  // Get the element design variables values
  int getDesignVars(int elemIndex, int dvLen, TacsScalar dvs[]);

  // Get the lower and upper bounds for the design variable values
  int getDesignVarRange(int elemIndex, int dvLen, TacsScalar lb[],
                        TacsScalar ub[]);

  // Evaluate the material density
  TacsScalar evalDensity(int elemIndex, const double pt[],
                         const TacsScalar X[]);

  // Add the derivative of the density
  void addDensityDVSens(int elemIndex, TacsScalar scale, const double pt[],
                        const TacsScalar X[], int dvLen, TacsScalar dfdx[]);

  // Evaluate the mass moments
  void evalMassMoments(int elemIndex, const double pt[], const TacsScalar X[],
                       TacsScalar moments[]);

  // Add the sensitivity of the mass moments
  void addMassMomentsDVSens(int elemIndex, const double pt[],
                            const TacsScalar X[], const TacsScalar scale[],
                            int dvLen, TacsScalar dfdx[]);

  // Evaluate the specific heat
  TacsScalar evalSpecificHeat(int elemIndex, const double pt[],
                              const TacsScalar X[]);

  // Evaluate the stresss
  void evalStress(int elemIndex, const double pt[], const TacsScalar X[],
                  const TacsScalar strain[], TacsScalar stress[]);

  // Evaluate the tangent stiffness
  void evalTangentStiffness(int elemIndex, const double pt[],
                            const TacsScalar X[], TacsScalar C[]);

  // Add the contribution
  void addStressDVSens(int elemIndex, TacsScalar scale, const double pt[],
                       const TacsScalar X[], const TacsScalar strain[],
                       const TacsScalar psi[], int dvLen, TacsScalar dfdx[]);

  // Calculate the point-wise failure criteria
  TacsScalar evalFailure(int elemIndex, const double pt[], const TacsScalar X[],
                         const TacsScalar e[]);

  // Evaluate the derivative of the failure criteria w.r.t. the strain
  TacsScalar evalFailureStrainSens(int elemIndex, const double pt[],
                                   const TacsScalar X[], const TacsScalar e[],
                                   TacsScalar sens[]);

  // Add the derivative of the failure criteria w.r.t. the design variables
  void addFailureDVSens(int elemIndex, TacsScalar scale, const double pt[],
                        const TacsScalar X[], const TacsScalar strain[],
                        int dvLen, TacsScalar dfdx[]);

  // The name of the constitutive object
  const char *getObjectName();

  // Retrieve the design variable for plotting purposes
  TacsScalar evalDesignFieldValue(int elemIndex, const double pt[],
                                  const TacsScalar X[], int index);

 private:
  TACSMaterialProperties *props;
  TacsScalar width, thickness;
  int width_num, thickness_num;
  TacsScalar lb_thickness, ub_thickness;
  TacsScalar lb_width, ub_width;
  TacsScalar ks_weight;
  // The object name
  static const char *constName;
};

#endif  // TACS_ISO_RECTANGLE_BEAM_CONSTITUTIVE_H
