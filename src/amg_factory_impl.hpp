#ifndef FILE_AMG_FACTORY_IMPL_HPP
#define FILE_AMG_FACTORY_IMPL_HPP

namespace amg
{

  /** --- Options --- **/

  template<class TMESH, class TM>
  struct AMGFactory<TMESH, TM> :: Options
  {
    /** Level-control **/
    size_t max_meas = 50;                // maximal maesure of coarsest mesh
    double aaf = 0.1;                    // assemble after factor
    double first_aaf = 0.05;             // (smaller) factor for first level. -1 for dont use
    double aaf_scale = 1;                // scale aaf, e.g if 2:   first_aaf, aaf, 2*aaf, 4*aaf, .. (or aaf, 2*aaf, ...)
    
    /** Discard - not back yet  **/

    /** Contract (Re-Distribute) **/
    bool enable_ctr = true;
    /** WHEN to contract **/
    double ctraf = 0.05;                 // contract after reducing measure by this factor
    double first_ctraf = 0.025;          // see first_aaf
    double ctraf_scale = 1;              // see aaf_scale
    double ctr_crs_thresh = 0.7;         // if coarsening slows down more than this, contract one step
    /** HOW AGGRESSIVELY to contract **/
    double ctr_pfac = 0.25;              // reduce active NP by this factor (ctr_pfac / ctraf should be << 1 !)
    /** Constraints for contract **/
    size_t ctr_min_nv = 500;             // re-distribute such that at least this many NV per proc remain
    size_t ctr_seq_nv = 500;             // re-distribute to sequential once NV reaches this threshhold
      
    /** Build a new mesh from a coarse level matrix**/
    bool enable_rbm = false;             // probably only necessary on coarse levels
    double rbmaf = 0.01;                 // rebuild mesh after measure decreases by this factor
    double first_rbmaf = 0.005;          // see first_aaf
    double rbmaf_scale = 1;              // see aaf_scale
    std::function<shared_ptr<TMESH>(shared_ptr<TMESH>, shared_ptr<TSPM_TM>)> rebuild_mesh =
      [](shared_ptr<TMESH> mesh, shared_ptr<TSPM_TM> mat) { return mesh; };
  };


  template<class TMESH, class TM>
  void AMGFactory<TMESH, TM> :: SetOptionsFromFlags (Options& opts, const Flags & flags, string prefix) const
  {
    // TODO: implement this ...
  }


  template<NODE_TYPE NT, class TMESH, class TM>
  struct NodalAMGFactory<NT, TMESH, TM> :: Options : public AMGFactory<TMESH, TM>::Options
  {

  };


  template<NODE_TYPE NT, class TMESH, class TM>
  void NodalAMGFactory<NT, TMESH, TM> :: SetOptionsFromFlags (Options& opts, const Flags & flags, string prefix) const
  {
    BASE :: SetOptionsFromFlags(opts, flags, prefix);
  }


  template<class FACTORY_CLASS, class TMESH, class TM>
  struct VertexBasedAMGFactory<FACTORY_CLASS, TMESH, TM> :: Options : public NodalAMGFactory<NT_VERTEX, TMESH, TM>::Options
  {

  };


  template<class FACTORY_CLASS, class TMESH, class TM>
  void VertexBasedAMGFactory<FACTORY_CLASS, TMESH, TM> :: SetOptionsFromFlags (Options& opts, const Flags & flags, string prefix) const
  {
    BASE :: SetOptionsFromFlags (opts, flags, prefix);
  }


  /** --- State --- **/


  template<class TMESH, class TM>
  struct AMGFactory<TMESH> :: State
  {
    /** Contract **/
    bool first_ctr_used = false;
    size_t last_nv_ctr;

    /** Rebuild Mesh **/
    bool first_rbm_used = false;
    size_t last_meas_rbm;
  };
  
  
  /** --- AMGFactory --- **/


  template<class TMESH, class TM>
  AMGFactory<TMESH, TM> :: AMGFactory (shared_ptr<TMESH> _finest_mesh, shared_ptr<BaseDOFMapStep> _embed_step, shared_ptr<Options> _opts)
    : opts(_opts), finest_mesh(_finest_mesh), embed_step(_embed_step), state(make_shared<State>())
  { ; }


  template<class TMESH, class TM>
  Array<shared_ptr<BaseSparseMatrix>> AMGFactory<TMESH, TM> :: RSU (Capsule cap, shared_ptr<DOFMap> dof_map)
  {
    
    shared_ptr<BaseDOFMapStep> ds;

    shared_ptr<TMESH> fmesh = capsule.mesh, cmesh = capsule.mesh;
    shared_ptr<ParallelDofs> fpds = capsule.pardofs, cpds = nullptr;
    shared_ptr<BaseSparseMatrix> cmat = capsule.mat;
    size_t curr_meas = GetMeshMeasure(*fmesh);

    if (cap.level == 0) { // (re-) initialize book-keeping
      state->first_ctr_used = state->first_rbm_used = false;
      state->last_nv_ctr = fmesh->template GetNNGlobal<NT_VERTEX>();
      state->last_meas_rbm = curr_mesh;
    }

    double af = (cap.level == 0) ? options->first_aaf : ( pow(options_aaf_scale, cap.level - (options->first_aaf == -1) ? 1 : 0) * options->aaf );
    size_t goal_meas = max( min(af, 0.9) * curr_meas, max(options->max_meas, 1));

    INT<3> level = { cap.level, 0, 0 }; // coarse / sub-coarse / ctr 

    Array<shared_ptr<BaseDOFMapStep>> sub_steps;
    shared_ptr<TSPM_TM> conc_pwp;

    double crs_meas_fac = 1;

    while (curr_meas > goal_meas) {

      while (curr_meas > goal_meas) {

	auto grid_step = BuildCoarseMap (cmesh, (level[0] == level[1] == 0) ? finest_free_verts : nullptr );

	if (grid_step == nullptr)
	  { break; }


	auto _cmesh = const_pointer_cast<TMESH>(gstep->GetMappedMesh());

	crs_meas_fac = ComputeMeasure(_cmesh) / (1.0 * curr_meas);

	if (crs_meas_fac > 0.95)
	  { throw Exception("no proper check in place here"); }

	bool non_acceptable = false; // i guess implement sth like that at some point ?
	if (non_acceptable)
	  { break; }
s
	cmesh = _cmesh;

	auto pwp = BuildPWProl(gstep, fpds);

	conc_prol = (conc_prol == nullptr) ? pwp : MatMultAB(*conc_prol, *pwp);
	cpds = BuildParallelDofs(fmesh);

	curr_meas = ComputeMeasure(*cmesh);

	level[1]++;
      } // inner while - we PROBABLY have a coarse map

      if(conc_prol != nullptr) {
	
	fpds = cpds; conc_pwp = nullptr; fmesh = cmesh;
	auto pstep = make_shared<ProlMap<TSPM_TM>> (pwp, fpds, cpds, true);
	SmoothProlongation(pstep, capsule.mesh);

	if (embed_step != nullptr) {
	  auto real_step = embed_step->Concatenate(pstep);
	  sub_steps.Append(real_step);
	  embed_step = nullptr; 
	}
	else { sub_steps.Append(pstep); }

	cmat = sub_steps.Last()->AssembleMatrix(cmat);
      }
      else if (!options->enable_ctr) { // throw exceptions here for now, but in principle just break is also fine i think
	throw Exception("Could not coarsen, and cannot contract (it is disabled)");
	// break;
      }
      else if (cmesh->GetEQCHierarchy()->GetCommunicator().Size() == 2) {
	throw Exception("Could not coarsen, and cannot contract (only 2 NP left!)");
	// break;
      }
      
      if (options->enable_ctr) { // if we are stuck coarsening wise, or if we have reached a threshhold, redistribute
	double af = (cap.level == 0) ? options->first_ctraf : ( pow(options_ctraf_scale, cap.level - (options->first_ctraf == -1) ? 1 : 0) * options->ctraf );
	size_t goal_nv = max( min(af, 0.9) * state->last_ctr_nv, max(options->ctr_seq_nv, 1));
	if ( (crs_meas_fac > options->ctr_crs_thresh) ||
	     (goal_nv > cmesh->GetNNGlobal<NT_VERTEX>() ) ) {

	  double fac = options->ctr_pfac;
	  auto ccomm = cmesh->GetEQCHierarchy()->GetCommunicator();
	  double ctr_factor = (cmesh->GetNNGlobal<NT_VERTEX>() < options->ctr_seq_nv) ? -1 :
	    min2(fac, double(next_nv) / options->ctr_min_nv / ccomm.Size());
	  
	  auto ctr_map = BuildContractMap(ctr_factor, cmesh);

	  cmesh = ctr_map->GetMappedMesh();

	  auto ds = BuildDOFContractMap(ctr_map);
	  sub_steps.Append(ds);

	  cmat = ds->AssembleMatrix(cmat);
	}
      }
      else { break; } // outer loop not useful
    } // outher while

    if (sub_steps.Size() > 0) { // we were able to do some sort of step
      shared_ptr<BaseDOFMapStep> tot_step = (sub_steps.Size() > 1) ? make_shared<ConcDMS>(sub_steps) : sub_steps[0];
      dof_map->AddStep(tot_step);
    }
    
    // coarse level matrix

    if (options->enable_rbm) {
      double af = (cap.level == 0) ? options->first_rbmaf : ( pow(options_rbmaf_scale, cap.level - (options->first_rbmaf == -1) ? 1 : 0) * options->aaf );
      size_t goal_meas = max( min(af, 0.9) * state->last_meas_rbm, max(options->max_meas, 1));
      if (curr_meas < goal_meas)
	{ cmesh = options->rebuild_mesh(cmesh, cmat); state->last_meas_rbm = curr_meas; }
    }

    bool do_more_levels = (cmesh != nullptr) &&
      (cap.level < options->max_n_levels) &&
      (options->max_meas > GetMeshMeasure (*cmesh) );
    
    if (do_more_levels) {
      // recursively call setup
      cap.level++; cap.mesh = cmesh; cap.mat = cmat; cap.
      auto cmats = RSU( {cap.level + 1, cmesh, cmat, cpds, nullptr}, dof_map );
      cmats.Append(cmat);
      return cmats;
    }
    else if (cmat == cap.mat) { // I could not do anyting, stop coarsening here I guess
      return Array<shared_ptr<BaseSparseMatrix>> ({cmat});
    }
    else {
      // no more coarse levels
      return Array<shared_ptr<BaseSparseMatrix>> ({cmat});
    }

  } // AMGFactory::RSU


  template<class TMESH, class TM>
  shared_ptr<GridContractMap<TMESH>> AMGFactory<TMESH, TM> :: BuildContractMap (shared_ptr<TMESH> mesh) const
  {
    static Timer t("BuildContractMap"); RegionTimer rt(t);
    int n_groups;

    if (this->ctr_factor == -1 ) { n_groups = 2; }
    else { n_groups = 1 + std::round( (mesh->GetEQCHierarchy()->GetCommunicator().Size()-1) * this->ctr_factor) ; }
    n_groups = max2(2, n_groups); // dont send everything from 1 to 0 for no reason
    Table<int> groups = PartitionProcsMETIS (*mesh, n_groups);
    return make_shared<GridContractMap<TMESH>>(move(groups), mesh);
  }


  /** --- NodalAMGFactory --- **/
  

  template<NODE_TYPE NT, class TMESH, class TM> shared_ptr<BaseDOFMapStep>
  NodalAMGFactory<NT, TMESH, TM> :: BuildParDofs (shared_ptr<TMESH> amesh) const
  {
    const auto & mesh = *amesh;
    const auto & eqc_h = *mesh.GetEQCHierarchy();
    size_t neqcs = eqc_h.GetNEQCS();
    size_t ndof = mesh.template GetNN<NT_VERTEX>();
    TableCreator<int> cdps(ndof);
    for (; !cdps.Done(); cdps++) {
      for (auto eq : Range(neqcs)) {
	auto dps = eqc_h.GetDistantProcs(eq);
	auto verts = mesh.template GetENodes<NT>(eq);
	for (auto vnr : verts) {
	  for (auto p:dps) cdps.Add(vnr, p);
	}
      }
    }
    return make_shared<ParallelDofs> (eqc_h.GetCommunicator(), cdps.MoveTable(), DPN, false);
  }


  template<NODE_TYPE NT, class TMESH, class TM> shared_ptr<BaseDOFMapStep>
  NodalAMGFactory<NT, TMESH, TM> :: BuildDOFContractMap (shared_ptr<GridContractMap<TMESH>> cmap) const
  {
    auto fg = cmap->GetGroup();
    Array<int> group(fg.Size()); group = fg;
    Table<int> dof_maps;
    shared_ptr<ParallelDofs> cpd = nullptr;
    if (cmap->IsMaster()) {
      // const TMESH& cmesh(*static_cast<const TMESH&>(*grid_step->GetMappedMesh()));
      shared_ptr<TMESH> cmesh = static_pointer_cast<TMESH>(cmap->GetMappedMesh());
      cpd = BuildParDofs(cmesh);
      Array<int> perow (group.Size()); perow = 0;
      for (auto k : Range(group.Size())) perow[k] = cmap->template GetNodeMap<NT>(k).Size();
      dof_maps = Table<int>(perow);
      for (auto k : Range(group.Size())) dof_maps[k] = cmap->template GetNodeMap<NT>(k);
    }
    auto ctr_map = make_shared<CtrMap<typename strip_vec<Vec<DPN, double>>::type>> (fpd, cpd, move(group), move(dof_maps));
    if (cmap->IsMaster()) {
      ctr_map->_comm_keepalive_hack = cmap->GetMappedEQCHierarchy()->GetCommunicator();
    }
    return move(ctr_map);
  }


  /** --- VertexBasedAMGFactory --- **/


  template<class FACTORY_CLASS, class TMESH, class TM>
  size_t VertexBasedAMGFactory<FACTORY_CLASS, TMESH, TM> :: GetMeshMeasure (TMESH & m) const
  {
    return m.GetNNGlobal<NT_VERTEX>();
  }


  template<class FACTORY_CLASS, class TMESH, class TM>
  shared_ptr<CoarseMap<TMESH>> VertexBasedAMGFactory<FACTORY_CLASS, TMESH, TM> :: BuildCoarseMap  (shared_ptr<TMESH> mesh) const
  {
    static Timer t("BuildCoarseMap"); RegionTimer rt(t);
    auto coarsen_opts = make_shared<typename HierarchicVWC<TMESH>::Options>();
    coarsen_opts->free_verts = free_verts;
    SetCoarseningOptions(*coarsen_opts, mesh);
    shared_ptr<VWiseCoarsening<TMESH>> calg;
    calg = make_shared<BlockVWC<TMESH>> (coarsen_opts);
    return calg->Coarsen(mesh);
  }


  template<class FACTORY_CLASS, class TMESH, class TM>
  shared_ptr<TSPM_TM> VertexBasedAMGFactory<FACTORY_CLASS, TMESH, TM> :: BuildPWProl (shared_ptr<GridConctractMap<TMESH>> cmap, shared_ptr<ParallelDofs> fpd) const
  {
    const FACTORY_CLASS & self = static_cast<const AMG_CLASS&>(*this);
    const CoarseMap<TMESH> & cmap(*_cmap);
    const TMESH & fmesh = static_cast<TMESH&>(*cmap.GetMesh()); fmesh.CumulateData();
    const TMESH & cmesh = static_cast<TMESH&>(*cmap.GetMappedMesh()); cmesh.CumulateData();

    size_t NV = fmesh.template GetNN<NT_VERTEX>();
    size_t NCV = cmesh.template GetNN<NT_VERTEX>();

    // Alloc Matrix
    auto vmap = cmap.template GetMap<NT_VERTEX>();
    Array<int> perow (NV); perow = 0;
    // -1 .. cant happen, 0 .. locally single, 1+ ..locally merged
    // -> cumulated: 0..single, 1+..merged
    Array<int> has_partner (NCV); has_partner = -1;
    for (auto vnr : Range(NV)) {
      auto cvnr = vmap[vnr];
      if (cvnr != -1)
	{ has_partner[cvnr]++; }
    }
    cmesh.template AllreduceNodalData<NT_VERTEX, int>(has_partner, [](auto & tab){ return move(sum_table(tab)); });
    for (auto vnr : Range(NV))
      { if (vmap[vnr] != -1) perow[vnr] = 1; }
    auto prol = make_shared<TSPM_TM>(perow, NCV);

    // Fill Matrix
    for (auto vnr : Range(NV)) {
      if (vmap[vnr]!=-1) {
	auto ri = prol->GetRowIndices(vnr);
	auto rv = prol->GetRowValues(vnr);
	auto cvnr = vmap[vnr];
	ri[0] = cvnr;
	if (has_partner[cvnr]==0) { // single vertex
	  SetIdentity(rv[0]);
	}
	else { // merged vertex
	  self.CalcPWPBlock (fmesh, cmesh, cmap, vnr, cvnr, rv[0]);
	}
      }
    }

    // cout << "PWP MAT: " << endl;
    // print_tm_spmat(cout, *prol); cout << endl<< endl;

    return prol;
  } // VertexBasedAMGFactory::BuildPWProl


  template<class FACTORY_CLASS, class TMESH, class TM>
  void VertexBasedAMGFactory<FACTORY_CLASS, TMESH, TM> :: SmoothProlongation (shared_ptr<ProlMap<TSPM_TM>> pmap, shared_ptr<TMESH> mesh) const
  {
    static Timer t("SmoothProlongation"); RegionTimer rt(t);

    const FACTORY_CLASS & self = static_cast<const AMG_CLASS&>(*this);
    const TMESH & fmesh(*afmesh); fmesh.CumulateData();
    const auto & fecon = *fmesh.GetEdgeCM();
    const auto & eqc_h(*fmesh.GetEQCHierarchy()); // coarse eqch==fine eqch !!
    const TSPM_TM & pwprol = *pmap->GetProl();

    const double MIN_PROL_FRAC = options->min_prol_frac;
    const int MAX_PER_ROW = options->max_per_row;
    const double omega = options->sp_omega;

    // Construct vertex-map from prol graph (can be concatenated)
    size_t NFV = fmesh.template GetNN<NT_VERTEX>();
    Array<size_t> vmap (NFV); vmap = -1;
    size_t NCV = 0;
    for (auto k : Range(NFV)) {
      auto ri = pwprol.GetRowIndices(k);
      // we should be able to combine smoothing and discarding
      // if (ri.Size() > 1) { cout << "TODO: handle this case, dummy" << endl; continue; } // this is for double smooted prol
      // if (ri.Size() > 1) { throw Exception("comment this out"); }
      if (ri.Size() > 0) {
	vmap[k] = ri[0];
	NCV = max2(NCV, size_t(ri[0]+1));
      }
    }

    // For each fine vertex, sum up weights of edges that connect to the same CV
    //  (can be more than one edge, if the pw-prol is concatenated)
    // TODO: for many concatenated pws, does this dominate edges to other agglomerates??
    auto all_fedges = fmesh.template GetNodes<NT_EDGE>();
    Array<double> vw (NFV); vw = 0;
    auto neqcs = eqc_h.GetNEQCS();
    {
      INT<2, int> cv;
      auto doit = [&](auto the_edges) {
	for (const auto & edge : the_edges) {
	  if ( ((cv[0]=vmap[edge.v[0]]) != -1 ) &&
	       ((cv[1]=vmap[edge.v[1]]) != -1 ) &&
	       (cv[0]==cv[1]) ) {
	    auto com_wt = self.template GetWeight<NT_EDGE>(fmesh, edge);
	    vw[edge.v[0]] += com_wt;
	    vw[edge.v[1]] += com_wt;
	  }
	}
      };
      for (auto eqc : Range(neqcs)) {
	if (!eqc_h.IsMasterOfEQC(eqc)) continue;
	doit(fmesh.template GetENodes<NT_EDGE>(eqc));
	doit(fmesh.template GetCNodes<NT_EDGE>(eqc));
      }
    }
    fmesh.template AllreduceNodalData<NT_VERTEX>(vw, [](auto & tab){return move(sum_table(tab)); }, false);

    /** Find Graph for Prolongation **/
    Table<int> graph(NFV, MAX_PER_ROW); graph.AsArray() = -1; // has to stay
    Array<int> perow(NFV); perow = 0; // 
    {
      Array<INT<2,double>> trow;
      Array<INT<2,double>> tcv;
      Array<size_t> fin_row;
      for (auto V:Range(NFV)) {
	auto CV = vmap[V];
	if ( is_invalid(CV) ) continue; // grounded -> TODO: do sth. here if we are free?
	if (vw[V] == 0.0) { // MUST be single
	  perow[V] = 1;
	  graph[V][0] = CV;
	  continue;
	}
	trow.SetSize(0);
	tcv.SetSize(0);
	auto EQ = fmesh.template GetEqcOfNode<NT_VERTEX>(V);
	auto ovs = fecon.GetRowIndices(V);
	auto eis = fecon.GetRowValues(V);
	size_t pos;
	for (auto j:Range(ovs.Size())) {
	  auto ov = ovs[j];
	  auto cov = vmap[ov];
	  if (is_invalid(cov) || cov==CV) continue;
	  auto oeq = fmesh.template GetEqcOfNode<NT_VERTEX>(ov);
	  if (eqc_h.IsLEQ(EQ, oeq)) {
	    auto wt = self.template GetWeight<NT_EDGE>(fmesh, all_fedges[eis[j]]);
	    if ( (pos = tcv.Pos(cov)) == size_t(-1)) {
	      trow.Append(INT<2,double>(cov, wt));
	      tcv.Append(cov);
	    }
	    else {
	      trow[pos][1] += wt;
	    }
	  }
	}
	QuickSort(trow, [](const auto & a, const auto & b) {
	    if (a[0]==b[0]) return false;
	    return a[1]>b[1];
	  });
	double cw_sum = (is_valid(CV)) ? vw[V] : 0.0;
	fin_row.SetSize(0);
	if (is_valid(CV)) fin_row.Append(CV); //collapsed vertex
	size_t max_adds = (is_valid(CV)) ? min2(MAX_PER_ROW-1, int(trow.Size())) : trow.Size();
	for (auto j:Range(max_adds)) {
	  cw_sum += trow[j][1];
	  if (is_valid(CV)) {
	    // I don't think I actually need this: Vertex is collapsed to some non-weak (not necessarily "strong") edge
	    // therefore the relative weight comparison should eliminate all really weak connections
	    // if (fin_row.Size() && (trow[j][1] < MIN_PROL_WT)) break; 
	    if (trow[j][1] < MIN_PROL_FRAC*cw_sum) break;
	  }
	  fin_row.Append(trow[j][0]);
	}
	QuickSort(fin_row);
	perow[V] = fin_row.Size();
	for (auto j:Range(fin_row.Size()))
	  graph[V][j] = fin_row[j];
      }
    }
    
    /** Create Prolongation **/
    auto sprol = make_shared<TSPM_TM>(perow, NCV);

    /** Fill Prolongation **/
    LocalHeap lh(2000000, "Tobias", false); // ~2 MB LocalHeap
    Array<INT<2,size_t>> uve(30); uve.SetSize(0);
    Array<int> used_verts(20), used_edges(20);
    TMAT id; SetIdentity(id);
    for (int V:Range(NFV)) {
      auto CV = vmap[V];
      if (is_invalid(CV)) continue; // grounded -> TODO: do sth. here if we are free?
      if (perow[V] == 1) { // SINGLE or no good connections avail.
	sprol->GetRowIndices(V)[0] = CV;
	sprol->GetRowValues(V)[0] = pwprol.GetRowValues(V)[0];
      }
      else { // SMOOTH
	HeapReset hr(lh);
	// Find which fine vertices I can include
	auto EQ = fmesh.template GetEqcOfNode<NT_VERTEX>(V);
	auto graph_row = graph[V];
	auto all_ov = fecon.GetRowIndices(V);
	auto all_oe = fecon.GetRowValues(V);
	uve.SetSize(0);
	for (auto j:Range(all_ov.Size())) {
	  auto ov = all_ov[j];
	  auto cov = vmap[ov];
	  if (is_valid(cov)) {
	    if (graph_row.Contains(cov)) {
	      auto eq = fmesh.template GetEqcOfNode<NT_VERTEX>(ov);
	      if (eqc_h.IsLEQ(EQ, eq)) {
		uve.Append(INT<2>(ov,all_oe[j]));
	      } } } }
	uve.Append(INT<2>(V,-1));
	QuickSort(uve, [](const auto & a, const auto & b){return a[0]<b[0];}); // WHY??
	used_verts.SetSize(uve.Size()); used_edges.SetSize(uve.Size());
	for (auto k:Range(uve.Size()))
	  { used_verts[k] = uve[k][0]; used_edges[k] = uve[k][1]; }
	
	auto posV = find_in_sorted_array(int(V), used_verts);
      	size_t unv = used_verts.Size(); // # of vertices used
	FlatMatrix<TMAT> mat (1,unv,lh); mat(0, posV) = 0;
	FlatMatrix<TMAT> block (2,2,lh);
	for (auto l:Range(unv)) {
	  if (l==posV) continue;
	  self.CalcRMBlock (fmesh, all_fedges[used_edges[l]], block);
	  int brow = (V < used_verts[l]) ? 0 : 1;
	  mat(0,l) = block(brow,1-brow); // off-diag entry
	  mat(0,posV) += block(brow,brow); // diag-entry
	}

	TMAT diag;
	double tr = 1;
	if constexpr(mat_traits<TMAT>::HEIGHT == 1) {
	    diag = mat(0, posV);
	  }
	else {
	  diag = mat(0, posV);
	  tr = 0; Iterate<mat_traits<TMAT>::HEIGHT>([&](auto i) { tr += diag(i.value,i.value); });
	  tr /= mat_traits<TMAT>::HEIGHT;
	  diag /= tr;
	  // if (sing_diags) {
	  //   self.RegDiag(diag);
	  // }
	}
	CalcInverse(diag);
	
	auto sp_ri = sprol->GetRowIndices(V); sp_ri = graph_row;
	auto sp_rv = sprol->GetRowValues(V); sp_rv = 0;
	double fac = omega/tr;
	for (auto l : Range(unv)) {
	  int vl = used_verts[l];
	  auto pw_rv = pwprol.GetRowValues(vl);
	  int cvl = vmap[vl];
	  auto pos = find_in_sorted_array(cvl, sp_ri);
	  if (l==posV)
	    { sp_rv[pos] += pw_rv[0]; }
	  sp_rv[pos] -= fac * (diag * mat(0,l)) * pw_rv[0];

	}
      }
    }

    // cout << "SPROL MAT: " << endl;
    // print_tm_spmat(cout, *sprol); cout << endl;

    pmap->SetProl(sprol);
  } // VertexBasedAMGFactory::SmoothProlongation
  

} // namespace amg

#endif
