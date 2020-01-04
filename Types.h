/***********************************************************************
Tipos: Declaraciones de tipos de datos intercambiados entre los módulos
AR Sandbox.
Copyright (c) 2014 Oliver Kreylos

This file is part of the Augmented Reality Sandbox (SARndbox).

The Augmented Reality Sandbox is free software; you can redistribute it
and/or modify it under the terms of the GNU General Public License as
published by the Free Software Foundation; either version 2 of the
License, or (at your option) any later version.

The Augmented Reality Sandbox is distributed in the hope that it will be
useful, but WITHOUT ANY WARRANTY; without even the implied warranty of
MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
General Public License for more details.

You should have received a copy of the GNU General Public License along
with the Augmented Reality Sandbox; if not, write to the Free Software
Foundation, Inc., 59 Temple Place, Suite 330, Boston, MA 02111-1307 USA
***********************************************************************/

#ifndef TYPES_INCLUDED
#define TYPES_INCLUDED

#include <Geometry/Point.h>
#include <Geometry/Vector.h>
#include <Geometry/Plane.h>
#include <Geometry/OrthogonalTransformation.h>
#include <Geometry/ProjectiveTransformation.h>

typedef double Scalar; // Tipo escalar para objetos de geometría
typedef Geometry::Point<Scalar,3> Point; // Escriba para puntos afines 3D
typedef Geometry::Vector<Scalar,3> Vector; // Tipo para vectores afines 3D
typedef Geometry::Plane<Scalar,3> Plane; // Tipo para planos afines 3D
typedef Geometry::OrthogonalTransformation<Scalar,3> OGTransform; // Tipo para transformaciones de cuerpo rígido a escala 3D
typedef Geometry::ProjectiveTransformation<Scalar,3> PTransform; // Tipo para transformaciones proyectivas 3D (matrices 4x4)

#endif
