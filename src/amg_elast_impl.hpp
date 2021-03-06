#ifdef ELASTICITY
#ifndef FILE_AMG_ELAST_IMPL_HPP
#define FILE_AMG_ELAST_IMPL_HPP

#include "amg_elast.hpp"
#include "amg_energy_impl.hpp"
#include "amg_factory_nodal_impl.hpp"
#include "amg_factory_vertex_impl.hpp"
#include "amg_pc.hpp"
#include "amg_pc_vertex.hpp"
#include "amg_pc_vertex_impl.hpp"


/** Need this only if we also include the PC headers **/

namespace amg
{

  template<int DIM>
  void AttachedEED<DIM> :: map_data (const BaseCoarseMap & cmap, AttachedEED<DIM> & ceed) const
  {
    static Timer t(string("AttachedEED::map_data")); RegionTimer rt(t);
    auto & M = static_cast<ElasticityMesh<DIM>&>(*this->mesh); M.CumulateData();
    const auto & eqc_h = *M.GetEQCHierarchy();
    auto& fecon = *M.GetEdgeCM();
    ElasticityMesh<DIM> & CM = static_cast<ElasticityMesh<DIM>&>(*ceed.mesh);
    auto cedges = CM.template GetNodes<NT_EDGE>();
    const size_t NV = M.template GetNN<NT_VERTEX>();
    const size_t NE = M.template GetNN<NT_EDGE>();
    const size_t CNV = cmap.template GetMappedNN<NT_VERTEX>();
    const size_t CNE = cmap.template GetMappedNN<NT_EDGE>();
    auto e_map = cmap.template GetMap<NT_EDGE>();
    auto v_map = cmap.template GetMap<NT_VERTEX>();
    get<0>(M.Data())->Cumulate(); // should be cumulated anyways
    auto fvd = get<0>(M.Data())->Data();
    get<0>(CM.Data())->Cumulate(); // we need full vertex positions !
    auto cvd = get<0>(CM.Data())->Data();
    auto fed = this->Data();
    auto ced = ceed.Data();
    typedef Mat<BS, BS, double> TM;
    // TODO: we are modifying coarse v-wts here. HACKY!!
    Array<double> add_cvw (CNV); add_cvw = 0;
    Vec<DIM> posH, posh, tHh;
    TM TEST = 0;
    TM QHh(0), FMQ(0);
    ceed.data.SetSize(CNE); // ced. is a flat-array, so directly access ceed.data
    ced = 0.0;
    M.template ApplyEQ<NT_EDGE>([&] (auto eqc, const auto & fedge) LAMBDA_INLINE {
	auto cenr = e_map[fedge.id];
	if (cenr != -1) {
	  const auto & cedge = cedges[cenr];
	  auto& cemat = ced[cenr];
	  posH = 0.5 * (cvd[cedge.v[0]].pos + cvd[cedge.v[1]].pos);
	  posh = 0.5 * (fvd[fedge.v[0]].pos + fvd[fedge.v[1]].pos);
	  tHh = posh - posH;
	  ElasticityAMGFactory<DIM>::ENERGY::CalcQ(tHh, QHh);
	  FMQ = fed[fedge.id] * QHh;
	  cemat += Trans(QHh) * FMQ;
	}
	else { // add largest eval of connection to ground to the coarse vertex-weight
	  auto tr = calc_trace(fed[fedge.id]) / mat_traits<TM>::HEIGHT; // a guess
	  Iterate<2>([&](auto l) LAMBDA_INLINE {
	      if (v_map[fedge.v[1-l.value]] == -1) {
		auto cvnr = v_map[fedge.v[l.value]];
		// cout << l.value << ", v " << fedge.v[l.value] << " -> " << cvnr << endl;
		if (cvnr != -1)
		  { add_cvw[cvnr] += tr; }
	      }
	    });
	}
      }, true); // master only
    CM.template AllreduceNodalData<NT_VERTEX>(add_cvw, [](auto & in) { return sum_table(in); }, false);
    for (auto k : Range(CNV)) // cvd and add_cvw are both "CUMULATED"
      { cvd[k].wt += add_cvw[k]; }
    ceed.SetParallelStatus(DISTRIBUTED);
  } // AttachedEED::map_data

} // namespace amg

#ifdef FILE_AMG_ELAST_CPP

/** Need this only where we instantiate Elasticity PC **/

namespace amg
{

  template<class FACTORY, class HTVD, class HTED>
  void ElmatVAMG<FACTORY, HTVD, HTED> :: AddElementMatrix (FlatArray<int> dnums, const FlatMatrix<double> & elmat,
							   ElementId ei, LocalHeap & lh)
  { // TODO - get this from old version
    ;
  } // ElmatVAMG::AddElementMatrix


  template<class FCC>
  void VertexAMGPC<FCC> :: SetDefaultOptions (BaseAMGPC::Options& base_O)
  {
    auto & O(static_cast<Options&>(base_O));

    auto fes = bfa->GetFESpace();

    /** keep vertex positions! **/
    O.store_v_nodes = true;

    /** Coarsening Algorithm **/
    O.crs_alg = Options::CRS_ALG::AGG;
    O.ecw_geom = false;
    O.ecw_robust = false;
    O.n_levels_d2_agg = 1;
    O.agg_neib_boost = false; // might be worth it

    /** Smoothed Prolongation **/
    O.enable_sp = true;
    O.sp_needs_cmap = false;
    O.sp_min_frac = (ma->GetDimension() == 3) ? 0.08 : 0.15;
    O.sp_max_per_row = 1 + FCC::DIM;
    O.sp_omega = 1.0;

    /** Discard **/
    O.enable_disc = true;
    O.disc_max_bs = 1; // TODO: make this work

    /** Level-control **/
    O.enable_multistep = false;
    O.use_static_crs = true;
    O.first_aaf = (FCC::DIM == 3) ? 0.025 : 0.05;
    O.aaf = 1/pow(2, FCC::DIM);

    /** Redistribute **/
    O.enable_redist = true;
    O.rdaf = 0.05;
    O.first_rdaf = O.aaf * O.first_aaf;
    O.rdaf_scale = 1;
    O.rd_crs_thresh = 0.9;
    O.rd_min_nv_gl = 5000;
    O.rd_seq_nv = 5000;

    /** Smoothers **/
    O.sm_type = Options::SM_TYPE::GS;
    O.keep_grid_maps = false;
    O.gs_ver = Options::GS_VER::VER3;

    /** make a guess wether we have rotations or not, and how the DOFs are ordered **/
    std::function<Array<size_t>(shared_ptr<FESpace>)> check_space = [&](auto fes) -> Array<size_t> {
      auto fes_dim = fes->GetDimension();
      // cout << " fes " << typeid(*fes).name() << " dim " << fes_dim << endl;
      if (auto comp_fes = dynamic_pointer_cast<CompoundFESpace>(fes)) {
	auto n_spaces = comp_fes->GetNSpaces();
	Array<size_t> comp_bs;
	for (auto k : Range(n_spaces))
	  { comp_bs.Append(check_space((*comp_fes)[k])) ; }
	size_t sum_bs = std::accumulate(comp_bs.begin(), comp_bs.end(), 0);
	if (sum_bs == FCC::ENERGY::dofpv())
	  { O.with_rots = true; }
	else if (sum_bs == FCC::ENERGY::disppv())
	  { O.with_rots = false; }
	return comp_bs;
      }
      else if (auto reo_fes = dynamic_pointer_cast<ReorderedFESpace>(fes)) {
	// reordering changes [1,1,1] -> [3]
	auto base_space = reo_fes->GetBaseSpace();
	auto unreo_bs = check_space(base_space);
	size_t sum_bs = std::accumulate(unreo_bs.begin(), unreo_bs.end(), 0);
	if (sum_bs == FCC::ENERGY::dofpv())
	  { O.with_rots = true; }
	else if (sum_bs == FCC::ENERGY::disppv())
	  { O.with_rots = false; }
	return Array<size_t>({sum_bs});
      }
      else if (fes_dim == FCC::ENERGY::dofpv()) { // only works because mdim+compound does not work, so we never land here in the compound case
	O.with_rots = true;
	return Array<size_t>({1});
      }
      else if (fes_dim == FCC::ENERGY::disppv()) {
	O.with_rots = false;
	return Array<size_t>({1});
      }
      else
	{ return Array<size_t>({1}); }
    };
    O.block_s = check_space(bfa->GetFESpace());

    O.regularize_cmats = !O.with_rots; // without rotations on finest level, we can get singular coarse mats !

  } // VertexAMGPC::SetDefaultOptions


  template<class FCC>
  void VertexAMGPC<FCC> :: ModifyOptions (BaseAMGPC::Options & aO, const Flags & flags, string prefix)
  {
    auto & O(static_cast<Options&>(aO));
    if ( (O.sm_type == Options::SM_TYPE::BGS) ||
	 (O.spec_sm_types.Pos(Options::SM_TYPE::BGS) != -1) )
      { O.keep_grid_maps = true; }
  } // VertexAMGPC::ModifyOptions


  template<class FCC> template<class TD2V, class TV2D> shared_ptr<typename FCC::TMESH>
  VertexAMGPC<FCC> :: BuildAlgMesh_ALG_scal (shared_ptr<BlockTM> top_mesh, shared_ptr<BaseSparseMatrix> spmat,
					     TD2V D2V, TV2D V2D) const
  {
    static Timer ti("BuildAlgMesh_ALG_scal"); RegionTimer rt(ti);
    const auto & O(static_cast<Options&>(*options));

    const auto& dof_blocks(O.block_s);
    if ( (dof_blocks.Size() != 1) || (dof_blocks[0] != 1) )
      { throw Exception("block_s for compound, but called algmesh_alg_scal!"); }

    /** Vertex Data  **/
    auto a = new AttachedEVD<DIM>(Array<ElastVData<FCC::DIM>>(top_mesh->GetNN<NT_VERTEX>()), CUMULATED); // !! otherwise pos is garbage
    auto vdata = a->Data(); // TODO: get penalty dirichlet from row-sums (only taking x/y/z displacement entries)
    FlatArray<int> vsort = node_sort[NT_VERTEX];
    Vec<FCC::DIM> t; const auto & MA(*ma);
    for (auto k : Range(O.v_nodes)) {
      auto vnum = vsort[k];
      vdata[vnum].wt = 0;
      GetNodePos(O.v_nodes[k], MA, vdata[vnum].pos, t);
    }

    /** Edge Data  **/
    auto b = new AttachedEED<FCC::DIM>(Array<ElasticityEdgeData<FCC::DIM>>(top_mesh->GetNN<NT_EDGE>()), DISTRIBUTED); // !! has to be distr
    auto edata = b->Data();
    auto edges = top_mesh->GetNodes<NT_EDGE>();

    if (auto spm_tm = dynamic_pointer_cast<SparseMatrixTM<Mat<FCC::ENERGY::disppv(),FCC::ENERGY::disppv(),double>>>(spmat)) { // disp only
	const auto& A(*spm_tm);
	for (auto & e : edges) {
	  auto di = V2D(e.v[0]); auto dj = V2D(e.v[1]);
	  // cout << "edge " << e << endl << " dofs " << di << " " << dj << endl;
	  // cout << " mat etr " << endl; print_tm(cout, A(di, dj)); cout << endl;
	  // after BBDC, diri entries are compressed and mat has no entry (multidim BDDC doesnt work anyways)
	  double etrs = fabsum(A(di,dj));
	  // double fc = (ffds.Test(di) && ffds.Test(dj)) ? fabsum(A(di, dj)) / disppv(C::DIM) : 1e-4; // after BBDC, diri entries are compressed and mat has no entry 
	  double fc = (etrs != 0.0) ? etrs / FCC::ENERGY::disppv() : 1e-4;
	  // double fc = (ffds.Test(di) && ffds.Test(dj)) ? fabsum(A(di, dj)) / sqrt(fabsum(A(di,di)) * fabsum(A(dj,dj))) / disppv(C::DIM) : 1e-4;
	  auto & emat = edata[e.id]; emat = 0;
	  Vec<FCC::DIM> tang = vdata[e.v[1]].pos - vdata[e.v[0]].pos;
	  double len = L2Norm(tang);
	  fc /= (len * len);
	  Iterate<FCC::ENERGY::disppv()>([&](auto i) LAMBDA_INLINE {
	      Iterate<FCC::ENERGY::disppv()>([&](auto j) LAMBDA_INLINE {
		  emat(i.value, j.value) = fc * tang(i.value) * tang(j.value);
		});
	    });
	  auto fsem = fabsum(emat);
	  emat *= etrs / fsem;
    	}
    }
    else if (auto spm_tm = dynamic_pointer_cast<SparseMatrixTM<typename FCC::ENERGY::TM>>(spmat)) { // disp+rot
      const auto & ffds = *finest_freedofs;
      const auto & A(*spm_tm);
      for (auto & e : edges) {
	auto di = V2D(e.v[0]); auto dj = V2D(e.v[1]);
	// after BBDC, diri entries are compressed and mat has no entry (mult multidim BDDC doesnt work anyways)
	double fc = (ffds.Test(di) && ffds.Test(dj)) ? fabsum(A(di, dj)) / FCC::ENERGY::dofpv() : 1e-4; // after BBDC, diri entries are compressed and mat has no entry 
	// double fc = (ffds.Test(di) && ffds.Test(dj)) ? fabsum(A(di, dj)) / sqrt(fabsum(A(di,di)) * fabsum(A(dj,dj))) / dofpv(C::DIM) : 1e-4;
	auto & emat = edata[e.id]; emat = 0;
	Iterate<FCC::ENERGY::dofpv()>([&](auto i) LAMBDA_INLINE { emat(i.value, i.value) = fc; });
      }
    }
    else
      { throw Exception(string("not sure how to compute edge weights from mat of type ") + typeid(*spmat).name() + string("!")); }

    auto mesh = make_shared<typename FCC::TMESH>(move(*top_mesh), a, b);

    return mesh;
  } // VertexAMGPCBuildAlgMesh_ALG_scal


  template<class FCC> template<class TD2V, class TV2D> shared_ptr<typename FCC::TMESH>
  VertexAMGPC<FCC> :: BuildAlgMesh_ALG_blk (shared_ptr<BlockTM> top_mesh, shared_ptr<BaseSparseMatrix> spmat, TD2V D2V, TV2D V2D) const
  {
    static Timer ti("BuildAlgMesh_ALG_blk"); RegionTimer rt(ti);
    const auto & O(static_cast<Options&>(*options));

    auto spm_tm = dynamic_pointer_cast<SparseMatrixTM<double>>(spmat);
    if (spm_tm == nullptr)
      { throw Exception(string("not sure how to compute edge weights from mat of type (_blk version called)") + typeid(*spmat).name() + string("!")); }

    /** Vertex Data  **/
    auto a = new AttachedEVD<DIM>(Array<ElastVData<FCC::DIM>>(top_mesh->GetNN<NT_VERTEX>()), CUMULATED); // !! otherwise pos is garbage
    auto vdata = a->Data(); // TODO: get penalty dirichlet from row-sums (only taking x/y/z displacement entries)
    FlatArray<int> vsort = node_sort[NT_VERTEX];
    Vec<FCC::DIM> t; const auto & MA(*ma);
    for (auto k : Range(O.v_nodes)) {
      auto vnum = vsort[k];
      vdata[vnum].wt = 0;
      GetNodePos(O.v_nodes[k], MA, vdata[vnum].pos, t);
    }

    /** Edge Data  **/
    auto b = new AttachedEED<FCC::DIM>(Array<ElasticityEdgeData<FCC::DIM>>(top_mesh->GetNN<NT_EDGE>()), DISTRIBUTED); // !! has to be distr
    auto edata = b->Data();
    const auto& dof_blocks(O.block_s);
    auto edges = top_mesh->GetNodes<NT_EDGE>();
    const auto& ffds = *finest_freedofs;
    const auto & MAT = *spm_tm;

    for (const auto & e : edges) {
      auto dis = V2D(e.v[0]); auto djs = V2D(e.v[1]); auto diss = dis.Size();
      auto & ed = edata[e.id]; ed = 0;
      // after BBDC, diri entries are compressed and mat has no entry (mult multidim BDDC doesnt work anyways)
      if (ffds.Test(dis[0]) && ffds.Test(djs[0])) {
	double x = 0;
	// TODO: should I scale with diagonal inverse here ??
	// actually, i think i should scale with diag inv, then sum up, then scale back
	typename FCC::ENERGY::TM aij(0);
	for (auto i : Range(dis)) { // this could be more efficient
	  x += fabs(MAT(dis[i], djs[i]));
	  for (auto j = i+1; j < diss; j++)
	    { x += 2*fabs(MAT(dis[i], djs[j])); }
	}
	x /= (diss * diss);
	if (diss == FCC::ENERGY::disppv()) {
	  Vec<FCC::DIM> tang = vdata[e.v[1]].pos - vdata[e.v[0]].pos;
	  Iterate<FCC::ENERGY::disppv()>([&](auto i) LAMBDA_INLINE {
	      Iterate<FCC::ENERGY::disppv()>([&](auto j) LAMBDA_INLINE {
		  ed(i.value, j.value) = x * tang(i.value) * tang(j.value);
		});
	    });
	}
	else {
	  for (auto j : Range(diss))
	    { ed(j,j) = x; }
	}
      }
      else { // does not matter what we give in here, just dont want nasty NaNs below, however this is admittedly a bit hacky
	ed(0,0) = 0.00042;
      }
    }

    auto mesh = make_shared<typename FCC::TMESH>(move(*top_mesh), a, b);

    return mesh;
  } // VertexAMGPCBuildAlgMesh_ALG_blk


  template<class FCC> shared_ptr<typename FCC::TMESH>
  VertexAMGPC<FCC> :: BuildAlgMesh_TRIV (shared_ptr<BlockTM> top_mesh) const
  {
    static Timer ti("BuildAlgMesh_TRIV"); RegionTimer rt(ti);
    const auto & O(static_cast<Options&>(*options));
    auto a = new AttachedEVD<FCC::DIM>(Array<ElastVData<FCC::DIM>>(top_mesh->GetNN<NT_VERTEX>()), CUMULATED); // !! otherwise pos is garbage
    auto vdata = a->Data(); // TODO: get penalty dirichlet from row-sums (only taking x/y/z displacement entries)
    FlatArray<int> vsort = node_sort[NT_VERTEX];
    Vec<FCC::DIM> t; const auto & MA(*ma);
    for (auto k : Range(O.v_nodes)) {
      auto vnum = vsort[k];
      vdata[vnum].wt = 0;
      GetNodePos(O.v_nodes[k], MA, vdata[vnum].pos, t);
    }
    auto b = new AttachedEED<FCC::DIM>(Array<ElasticityEdgeData<FCC::DIM>>(top_mesh->GetNN<NT_EDGE>()), CUMULATED);
    for (auto & x : b->Data()) { SetIdentity(x); }
    auto mesh = make_shared<typename FCC::TMESH>(move(*top_mesh), a, b);
    return mesh;
  } // VertexAMGPC<FCC> :: BuildAlgMesh_TRIV


  template<> template<>
  shared_ptr<BaseDOFMapStep> INLINE VertexAMGPC<ElasticityAMGFactory<3>> :: BuildEmbedding_impl<2> (shared_ptr<TopologicMesh> mesh)
  { return nullptr; }


  template<class FCC> template<int BSA> shared_ptr<stripped_spm_tm<Mat<BSA, FCC::BS, double>>>
  VertexAMGPC<FCC> :: BuildED (size_t height, shared_ptr<TopologicMesh> mesh)
  {
    static_assert( (BSA == 1) || (BSA == FCC::ENERGY::disppv()) || (BSA == FCC::ENERGY::dofpv()),
		   "BuildED with nonsensical N !");
    const auto & O(static_cast<Options&>(*options));

    typedef stripped_spm_tm<Mat<BSA, FCC::BS, double>> TED;

    if (O.dof_ordering != Options::DOF_ORDERING::REGULAR_ORDERING)
      { throw Exception("BuildED only implemented for regular ordering"); }

    const auto & M(*mesh);

    if constexpr ( BSA == FCC::ENERGY::dofpv() ) // TODO: rot-flipping ?!
      { return nullptr; }
    else if constexpr ( BSA == FCC::ENERGY::disppv() ) { // disp -> disp,rot embedding
      if ( O.with_rots || (O.block_s.Size() != 1) || (O.block_s[0] != 1) )
	{ throw Exception("Elasticity BuildED: disp/disp+rot, block_s mismatch"); }
      Array<int> perow(M.template GetNN<NT_VERTEX>()); perow = 1;
      auto E_D = make_shared<TED>(perow, M.template GetNN<NT_VERTEX>());
      for (auto k : Range(perow)) {
	E_D->GetRowIndices(k)[0] = k;
	auto & v = E_D->GetRowValues(k)[0];
	v = 0; Iterate<FCC::ENERGY::disppv()>([&](auto i) { v(i.value, i.value) = 1; });
      }
      return E_D;
    }
    else if ( BSA == 1 ) {
      Array<int> perow(height); perow = 1;
      auto E_D = make_shared<TED>(perow, M.template GetNN<NT_VERTEX>());
      size_t row = 0, os_ri = 0;
      for (auto bs : O.block_s) {
	for (auto k : Range(M.template GetNN<NT_VERTEX>())) {
	  for (auto j : Range(bs)) {
	    E_D->GetRowIndices(row)[0] = k;
	    E_D->GetRowValues(row)[0] = 0;
	    E_D->GetRowValues(row)[0](os_ri + j) = 1;
	    row++;
	  }
	}
	os_ri += bs;
      }
      return E_D;
    }

  } // VertexAMGPC<FCC> :: BuildED

#ifdef FILE_AMG_ELAST_2D_CPP
  template<> void VertexAMGPC<ElasticityAMGFactory<2>> :: RegularizeMatrix (shared_ptr<BaseSparseMatrix> mat, shared_ptr<ParallelDofs> & pardofs) const
  {
    auto& A = static_cast<ElasticityAMGFactory<2>::TSPM_TM&>(*mat);
    if ( (pardofs != nullptr) && (pardofs->GetDistantProcs().Size() != 0) ) {
      Array<int> is_zero(A.Height());
      for(auto k : Range(A.Height()))
	{ is_zero[k] = (fabs(A(k,k)(2,2)) < 1e-10 ) ?  1 : 0; }
      AllReduceDofData(is_zero, MPI_SUM, pardofs);
      for(auto k : Range(A.Height()))
	if ( (pardofs->IsMasterDof(k)) && (is_zero[k] != 0) )
	  { A(k,k)(2,2) = 1; }
    }
    else {
      for(auto k : Range(A.Height())) {
	auto & diag_etr = A(k,k);
	if (fabs(diag_etr(2,2)) < 1e-8)
	  { diag_etr(2,2) = 1; }
      }
    }
  } // ElasticityAMGFactory<DIM>::RegularizeMatrix
#endif // FILE_AMG_ELAST_2D_CPP

#ifdef FILE_AMG_ELAST_3D_CPP
  template<> void VertexAMGPC<ElasticityAMGFactory<3>> :: RegularizeMatrix (shared_ptr<BaseSparseMatrix> mat, shared_ptr<ParallelDofs> & pardofs) const
  {
    auto& A = static_cast<ElasticityAMGFactory<3>::TSPM_TM&>(*mat);
    typedef ElasticityAMGFactory<3>::TM TM;
    if ( (pardofs != nullptr) && (pardofs->GetDistantProcs().Size() != 0) ) {
      Array<TM> diags(A.Height());
      for(auto k : Range(A.Height()))
	{ diags[k] = A(k,k); }
      MyAllReduceDofData(*pardofs, diags, [](auto & a, const auto & b) LAMBDA_INLINE { a+= b; });
      for(auto k : Range(A.Height())) {
	if (pardofs->IsMasterDof(k)) {
	  auto & dg_etr = A(k,k);
	  dg_etr = diags[k];
	  // cout << " REG DIAG " << k << endl;
	  // RegTM<3,3,6>(dg_etr); // might be buggy !?
	  RegTM<0,6,6>(dg_etr);
	  // cout << endl;
	}
	else
	  { A(k,k) = 0; }
      }
    }
    else {
      for(auto k : Range(A.Height())) {
	// cout << " REG DIAG " << k << endl;
	// RegTM<3,3,6>(A(k,k));
	RegTM<0,6,6>(A(k,k));
      }
    }
  } // ElasticityAMGFactory<DIM>::RegularizeMatrix
#endif // FILE_AMG_ELAST_3D_CPP

} // namespace amg

#endif

#endif
#endif // ELASTICITY
