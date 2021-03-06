#ifdef STOKES

#ifndef FILE_AMG_STOKES_HPP
#define FILE_AMG_STOKES_HPP

namespace amg
{

  /** Stokes AMG (grad-grad + div-div penalty):
      We assume that we have DIM DOFs per facet of the mesh. Divergence-The divergence 
       - DOFs are assigned to edges of the dual mesh.
   **/

  /** Laplace + div-div: displacements **/
  template<int D> struct StokesTM_TRAIT { typedef void type; };
  template<> struct StokesTM_TRAIT<2> { typedef Mat<2,2,double> type; };
  template<> struct StokesTM_TRAIT<3> { typedef Mat<3,3,double> type; };
  template<int D> using StokesTM = typename StokesTM_TRAIT<D>::type;


  // also has dynamic edge-edge mats?
  template<class TM, class MWD>
  class StokesMesh : public MWD
  {
  };

  template<int D>
  struct H1StokesVData
  {
    double area;
  }; // StokesH1VData


  // template<int D>
  // struct EpsEpsStokesVData
  // {
  //   Vec<D> pos;
  //   double area;
  // }; // StokesEpsVData


  template<int D>
  class AttachedSVD : public AttachedNodeData<NT_VERTEX, StokesVData<D>, AttachedSVD<D>>
  {
  public:
    using BASE = AttachedNodeData<NT_VERTEX, StokesVData<D>, AttachedSVD<D>>;
    using BASEL::map_data;

    AttachedSVD (Array<StokesVData<D>> && _data, PARALLEL_STATUS stat)
      : BASE(move(_data), stat)
    { ; }

    INLINE void map_data (const BaseCoarseMap & cmap, AttachedSVD & csvd) const
    {
      Cumulate();
      auto cdata = csvd.data; cdata.SetSize(map.GetMappedNN<NT>());
      cdata = 0;
      Array<int> cnt (cdata.Size()); cnt = 0;
      auto map = cmap.GetMap<NT_VERTEX>();
      mesh->Apply<NT_VERTEX>([&](auto v) {
	  auto cv = map[v];
	  if (cv != -1) {
	    cnt[cv]++;
	    cdata[cv].pos += data[v].pos;
	    cdata[cv].area += data[v].area;
	  }
	}, true);
      mesh->AllreduceNodalData<NT_VERTEX>(cnt, [](auto & in) { return move(sum_table(in)); } , false);
      for (auto k : Range(cdata))
	{ cdata[k].pos *= 1.0/cnt[k]; }
      csvd.SetParallelStatus(DISTRIBUTED);
    } // AttachedSVD::map_data

  }; // class AttachedSVD


  template<int DIM>
  struct H1StokesVData
  {
    Vec<DIM, double> pos;     // positon of vertex
    Vec<DIM, double> wt;      // weight for L2-part of the energy
    double area;              // area of vertex cell
  };

  template<int DIM>
  struct H1StokesEData
  {
    strip_map<Mat<DIM, DIM, double>> emat;    // edge-mat for h1-energy
    // double surf;                           // \int_F 1
    Mat<1, D> flow;                           // \int_F grad(\phi_j)\cdot n
  };


  template<int D>
  class AttachedSED : public AttachedEdgeData<NT_EDGE, StokesEData<D>, AttachedSED<D>>
  {
  public:
    using BASE = public AttachedEdgeData<NT_EDGE, StokesEData<D>, AttachedSED<D>>;
    using BASE::map_data;

    template<class TMESH> INLINE void map_data (const BaseCoarseMap & map, AttachedSED<D> & csed) const;

  }; // class AttachedSED


  template<int D>
  using H1StokesMesh = BlockAlgMesh<AttachedH1SVD<D>, AttachedH1SED<D>>;

  // template<int D>
  // using EpsEpsStokesMesh = BlockAlgMesh<AttachedEpsEpsSVD<D>, AttachedEpsEpsSED<D>>;




  // /** Stokes AMG Preconditioner for facet-nodal discretizations. E.g. from an auxiliary space.
  //     Does not directly work with HDiv spaces. **/
  // template<class TFACTORY>
  // class StokesAMGPC : public BaseAMGPC
  // {
  // public:
  //   struct Options; // TODO: this is not entirely consistent

  // protected:

  //   /** Stuff consistent with other AMG PC classes for facet auxiliary PC **/
  //   shared_ptr<Options> options;

  //   shared_ptr<BilinearForm> bfa;

  //   shared_ptr<BitArray> finest_freedofs, free_verts;
  //   shared_ptr<BaseMatrix> finest_mat;

  //   Array<Array<int>> node_sort;
  //   Array<Array<Vec<3,double>>> node_pos;

  //   bool use_v2d_tab = false;
  //   Array<int> d2v_array, v2d_array;
  //   //    Table<int> v2d_table; // probably never use this

  //   shared_ptr<AMGMatrix> amg_mat;

  //   shared_ptr<FACTORY> factory;

  // public:

  //   /** Constructors **/

  //   StokesAMGPC (shared_ptr<BilinearForm> bfa, const Flags & aflags, const string name = "precond");

  //   StokesAMGPC (const PDE & apde, const Flags & aflags, const string aname = "precond");


  //   /** New public methods **/


  //   /** Public methods - consistent with EmbedVAMG **/

  //   virtual shared_ptr<TMESH> BuildInitialMesh ();

  //   virtual shared_ptr<FACTORY> BuildFactory (shared_ptr<TMESH> mesh);

  //   virtual void BuildAMGMat ();


  //   /** Public methods inherited from Preconditioner **/

  //   virtual void InitLevel (shared_ptr<BitArray> freedofs = nullptr) override;

  //   virtual void FinalizeLevel (const BaseMatrix * mat) override;

  //   virtual void Update () override;


  // protected:

  //   /** Protected methods inherited from Preconditioner (I think none) **/

  //   /** New protected methods - internal utility**/

  //   virtual shared_ptr<TMESH> BuildInitialMesh ();

  //   virtual shared_ptr<BlockTM> BuildTopMesh (shared_ptr<EQCHierarchy> eqc_h);


  // }; // StokesAMGPC


} // namespace amg

#endif

#endif
