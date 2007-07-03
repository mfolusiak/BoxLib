//
// $Id: AuxBoundaryData.cpp,v 1.2 2007-05-01 02:59:57 lijewski Exp $
//
#include <winstd.H>

#include <AuxBoundaryData.H>

AuxBoundaryData::AuxBoundaryData ()
    :
    m_ngrow(0),
    m_initialized(false)
{}

AuxBoundaryData::AuxBoundaryData (const BoxArray& ba,
                                  int             n_grow,
                                  int             n_comp,
                                  const Geometry& geom)
    :
    m_ngrow(n_grow),
    m_initialized(false)
{
    initialize(ba,n_grow,n_comp,geom);
}

void
AuxBoundaryData::copy (const AuxBoundaryData& src,
                       int                    src_comp,
                       int                    dst_comp,
                       int                    num_comp)
{
    BL_ASSERT(m_initialized);
    BL_ASSERT(src_comp + num_comp <= src.m_fabs.nComp());
    BL_ASSERT(dst_comp + num_comp <= m_fabs.nComp());

    m_fabs.copy(src.m_fabs,src_comp,dst_comp,num_comp);
}

AuxBoundaryData::AuxBoundaryData (const AuxBoundaryData& rhs)
    :
    m_fabs(rhs.m_fabs.boxArray(),rhs.m_fabs.nComp(),0),
    m_ngrow(rhs.m_ngrow)
{
    m_fabs.copy(rhs.m_fabs,0,0,rhs.m_fabs.nComp());

    m_initialized = true;
}

void
AuxBoundaryData::initialize (const BoxArray& ba,
			     int             n_grow,
			     int             n_comp,
                             const Geometry& geom)
{
    BL_ASSERT(!m_initialized);

    m_ngrow = n_grow;
    //
    // First get list of all ghost cells.
    //
    BoxList gcells, bcells;

    for (int i = 0; i < ba.size(); ++i)
	gcells.join(BoxLib::boxDiff(BoxLib::grow(ba[i],n_grow),ba[i]));
    //
    // Now strip out intersections with original BoxArray.
    //
    for (BoxList::const_iterator it = gcells.begin(); it != gcells.end(); ++it)
    {
        std::vector< std::pair<int,Box> > isects = ba.intersections(*it);

        if (isects.empty())
            bcells.push_back(*it);
        else
        {
            //
            // Collect all the intersection pieces.
            //
            BoxList pieces;
            for (int i = 0; i < isects.size(); i++)
                pieces.push_back(isects[i].second);
            BoxList leftover = BoxLib::complementIn(*it,pieces);
            bcells.catenate(leftover);
        }
    }
    //
    // Now strip out overlaps.
    //
    gcells.clear();
    gcells = BoxLib::removeOverlap(bcells);
    bcells.clear();

    if (geom.isAnyPeriodic())
    {
        //
        // Remove any intersections with periodically shifted valid region.
        //
        Box dmn = geom.Domain();

        for (int d = 0; d < BL_SPACEDIM; d++)
            if (!geom.isPeriodic(d)) 
                dmn.grow(d,n_grow);

        for (BoxList::iterator it = gcells.begin(); it != gcells.end(); )
        {
            const Box isect = *it & dmn;

            if (isect.ok())
                *it++ = isect;
            else
                gcells.remove(it++);
        }
    }

    BoxArray nba(gcells);

    std::vector<long> wgts(nba.size());

    for (unsigned int i = 0; i < wgts.size(); i++)
    {
        wgts[i] = nba[i].numPts();
    }
    DistributionMapping dm;
    //
    // This call doesn't invoke the MinimizeCommCosts() stuff.
    // There's very little to gain with this type of covering.
    // This also guarantees that this DM won't be put into the cache.
    //
    dm.KnapSackProcessorMap(wgts,ParallelDescriptor::NProcs());

    m_fabs.define(nba, n_comp, 0, dm, Fab_allocate);

    m_initialized = true;
}

void
AuxBoundaryData::copyTo (MultiFab& mf,
                         int       src_comp,
                         int       dst_comp,
                         int       num_comp) const
{
    BL_ASSERT(m_initialized);

    mf.copy(m_fabs,src_comp,dst_comp,num_comp);
}

void
AuxBoundaryData::copyFrom (const MultiFab& mf,
                           int       src_comp,
                           int       dst_comp,
                           int       num_comp)
{
    BL_ASSERT(m_initialized);

    m_fabs.copy(mf,src_comp,dst_comp,num_comp);
}