#ifndef FILE_AMG_ENERGY_HPP
#define FILE_AMG_ENERGY_HPP

namespace amg
{

  template<int ADIM, class T_V_DATA, class T_E_DATA>
  class H1Energy
  {
  public:
    using TVD = T_V_DATA;
    using TED = T_E_DATA;
    // static constexpr int DPV () { return DIM; }
    static constexpr int DIM = ADIM;
    static constexpr int DPV = ADIM;
    static constexpr bool NEED_ROBUST = false;
    typedef typename strip_mat<Mat<DIM,DIM,double>>::type TM;

    static INLINE double GetApproxWeight (const TED & ed) { return ed; }

    static INLINE void CalcQij (const TVD & di, const TVD & dj, TM & Qij);
    static INLINE void ModQij (const TVD & di, const TVD & dj, TM & Qij);
    static INLINE void CalcQHh (const TVD & dH, const TVD & dh, TM & QHh);
    static INLINE void ModQHh (const TVD & dH, const TVD & dh, TM & QHh);
    static INLINE void CalcQs  (const TVD & di, const TVD & dj, TM & Qij, TM & Qji);
    static INLINE void ModQs  (const TVD & di, const TVD & dj, TM & Qij, TM & Qji);
    static INLINE TVD CalcMPData (const TVD & da, const TVD & db);

    static INLINE void CalcRMBlock (FlatMatrix<TM> mat, const TED & ed, const TVD & vdi, const TVD & vdj);
  };


#ifdef ELASTICITY

  template<int ADIM, class T_V_DATA, class T_E_DATA>
  class EpsEpsEnergy
  {
  public:
    using TVD = T_V_DATA;
    using TED = T_E_DATA;
    static constexpr int DIM = ADIM;
#ifdef ELASTICITY_ROBUST_ECW
    static constexpr bool NEED_ROBUST = true;
#else
    static constexpr bool NEED_ROBUST = false;
#endif
    static constexpr int dofpv () { return (DIM == 2) ? 3 : 6; }
    static constexpr int disppv () { return DIM; }
    static constexpr int rotpv () { return (DIM == 2) ? 1 : 3; }
    // static constexpr int DPV () { return (DIM == 2) ? 3 : 6; }
    static constexpr int DPV = dofpv();
    typedef Mat<DPV, DPV, double> TM;

    static INLINE double GetApproxWeight (const TED & ed) { return calc_trace(ed) / DPV; }

    static INLINE void CalcQ  (const Vec<DIM> & t, TM & Q);
    static INLINE void ModQ  (const Vec<DIM> & t, TM & Q);
    static INLINE void CalcQij (const TVD & di, const TVD & dj, TM & Qij);
    static INLINE void ModQij (const TVD & di, const TVD & dj, TM & Qij);
    static INLINE void CalcQHh (const TVD & dH, const TVD & dh, TM & QHh);
    static INLINE void ModQHh (const TVD & dH, const TVD & dh, TM & QHh);
    static INLINE void CalcQs  (const TVD & di, const TVD & dj, TM & Qij, TM & Qji);
    static INLINE void ModQs  (const TVD & di, const TVD & dj, TM & Qij, TM & Qji);
    static INLINE TVD CalcMPData (const TVD & da, const TVD & db);

    static INLINE void CalcRMBlock (FlatMatrix<TM> mat, const TED & ed, const TVD & vdi, const TVD & vdj);
  };

#endif

} // namespac amg

#endif
