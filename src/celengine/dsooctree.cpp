// dsooctree.cpp
//
// Description:
//
// Copyright (C) 2005-2009, Celestia Development Team
// Original version by Toti <root@totibox>
//
// This program is free software; you can redistribute it and/or
// modify it under the terms of the GNU General Public License
// as published by the Free Software Foundation; either version 2
// of the License, or (at your option) any later version.

#include "dsooctree.h"

#include <celastro/astro.h>
#include <celcompat/numbers.h>

namespace celestia::engine
{

// total specialization of the StaticOctree template process*() methods for DSOs:
template<>
void
DSOOctree::processVisibleObjects(DSOHandler& processor,
                                 const PointType& obsPosition,
                                 const PlaneType* frustumPlanes,
                                 float limitingFactor,
                                 double scale) const
{
    // See if this node lies within the view frustum

    // Test the cubic octree node against each one of the five
    // planes that define the infinite view frustum.
    for (unsigned int i = 0; i < 5; ++i)
    {
        const PlaneType& plane = frustumPlanes[i];

        double r = scale * plane.normal().cwiseAbs().sum();
        if (plane.signedDistance(m_cellCenterPos) < -r)
            return;
    }

    // Compute the distance to node; this is equal to the distance to
    // the cellCenterPos of the node minus the boundingRadius of the node, scale * SQRT3.
    double minDistance = (obsPosition - m_cellCenterPos).norm() - scale * numbers::sqrt3;

    // Process the objects in this node
    double dimmest = minDistance > 0.0 ? astro::appToAbsMag((double) limitingFactor, minDistance) : 1000.0;

    for (std::uint32_t i = 0; i < m_nObjects; ++i)
    {
        const auto& obj = m_firstObject[i];
        float absMag = obj->getAbsoluteMagnitude();
        if (absMag < dimmest)
        {
            double distance = (obsPosition - obj->getPosition()).norm() - obj->getBoundingSphereRadius();
            auto appMag = static_cast<float>((distance >= 32.6167) ? astro::absToAppMag(static_cast<double>(absMag), distance) : absMag);

            if (appMag < limitingFactor)
                processor.process(obj, distance, absMag);
        }
    }

    // See if any of the objects in child nodes are potentially included
    // that we need to recurse deeper.
    if (m_children != nullptr &&
        (minDistance <= 0.0 || astro::absToAppMag(static_cast<double>(m_exclusionFactor), minDistance) <= limitingFactor))
    {
        // Recurse into the child nodes
        for (int i = 0; i < 8; ++i)
        {
            (*m_children)[i]->processVisibleObjects(processor,
                                                    obsPosition,
                                                    frustumPlanes,
                                                    limitingFactor,
                                                    scale * 0.5f);
        }
    }
}

template<>
void
DSOOctree::processCloseObjects(DSOHandler& processor,
                               const PointType& obsPosition,
                               double boundingRadius,
                               double scale) const
{
    // Compute the distance to node; this is equal to the distance to
    // the cellCenterPos of the node minus the boundingRadius of the node, scale * SQRT3.
    double nodeDistance = (obsPosition - m_cellCenterPos).norm() - scale * numbers::sqrt3;

    if (nodeDistance > boundingRadius)
        return;

    // At this point, we've determined that the cellCenterPos of the node is
    // close enough that we must check individual objects for proximity.

    // Compute distance squared to avoid having to sqrt for distance
    // comparison.
    double radiusSquared = boundingRadius * boundingRadius;

    // Check all the objects in the node.
    for (std::uint32_t i = 0; i < m_nObjects; ++i)
    {
        const auto& obj = m_firstObject[i];
        PointType offset = obsPosition - obj->getPosition();
        if (offset.squaredNorm() < radiusSquared)
        {
            float  absMag   = obj->getAbsoluteMagnitude();
            double distance = offset.norm() - obj->getBoundingSphereRadius();
            processor.process(obj, distance, absMag);
        }
    }

    // Recurse into the child nodes
    if (m_children != nullptr)
    {
        for (int i = 0; i < 8; ++i)
        {
            (*m_children)[i]->processCloseObjects(processor,
                                                  obsPosition,
                                                  boundingRadius,
                                                  scale * 0.5f);
        }
    }
}

} // end namespace celestia::engine
