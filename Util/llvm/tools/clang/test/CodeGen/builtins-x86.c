// RUN: clang-cc -DUSE_64 -triple x86_64-unknown-unknown -emit-llvm -o %t %s
// RUN: clang-cc -DUSE_ALL -triple x86_64-unknown-unknown -fsyntax-only -o %t %s

#ifdef USE_ALL
#define USE_3DNOW
#define USE_64
#define USE_SSE4
#endif

// 64-bit
typedef char V8c __attribute__((vector_size(8 * sizeof(char))));
typedef signed short V4s __attribute__((vector_size(8)));
typedef signed int V2i __attribute__((vector_size(8)));
typedef signed long long V1LLi __attribute__((vector_size(8)));

typedef float V2f __attribute__((vector_size(8)));

// 128-bit
typedef char V16c __attribute__((vector_size(16)));
typedef signed short V8s __attribute__((vector_size(16)));
typedef signed int V4i __attribute__((vector_size(16)));
typedef signed long long V2LLi __attribute__((vector_size(16)));

typedef float V4f __attribute__((vector_size(16)));
typedef double V2d __attribute__((vector_size(16)));

void f0() {
  signed char         tmp_c;
//  unsigned char       tmp_Uc;
  signed short        tmp_s;
#ifdef USE_ALL
  unsigned short      tmp_Us;
#endif
  signed int          tmp_i;
  unsigned int        tmp_Ui;
  signed long long    tmp_LLi;
//  unsigned long long  tmp_ULLi;
  float               tmp_f;
  double              tmp_d;

  void*          tmp_vp;
  const void*    tmp_vCp;
  char*          tmp_cp; 
  const char*    tmp_cCp; 
  int*           tmp_ip;
  float*         tmp_fp;
  const float*   tmp_fCp;
  double*        tmp_dp;
  const double*  tmp_dCp;

#define imm_i 32
#define imm_i_0_2 0
#define imm_i_0_4 3
#define imm_i_0_8 7
#define imm_i_0_16 15
  // Check this.
#define imm_i_0_256 0

  V2i*   tmp_V2ip;
  V1LLi* tmp_V1LLip;
  V2LLi* tmp_V2LLip;

  // 64-bit
  V8c    tmp_V8c;
  V4s    tmp_V4s;
  V2i    tmp_V2i;
  V1LLi  tmp_V1LLi;
#ifdef USE_3DNOW
  V2f    tmp_V2f;
#endif

  // 128-bit
  V16c   tmp_V16c;
  V8s    tmp_V8s;
  V4i    tmp_V4i;
  V2LLi  tmp_V2LLi;
  V4f    tmp_V4f;
  V2d    tmp_V2d;

  tmp_i = __builtin_ia32_comieq(tmp_V4f, tmp_V4f);
  tmp_i = __builtin_ia32_comilt(tmp_V4f, tmp_V4f);
  tmp_i = __builtin_ia32_comile(tmp_V4f, tmp_V4f);
  tmp_i = __builtin_ia32_comigt(tmp_V4f, tmp_V4f);
  tmp_i = __builtin_ia32_comige(tmp_V4f, tmp_V4f);
  tmp_i = __builtin_ia32_comineq(tmp_V4f, tmp_V4f);
  tmp_i = __builtin_ia32_ucomieq(tmp_V4f, tmp_V4f);
  tmp_i = __builtin_ia32_ucomilt(tmp_V4f, tmp_V4f);
  tmp_i = __builtin_ia32_ucomile(tmp_V4f, tmp_V4f);
  tmp_i = __builtin_ia32_ucomigt(tmp_V4f, tmp_V4f);
  tmp_i = __builtin_ia32_ucomige(tmp_V4f, tmp_V4f);
  tmp_i = __builtin_ia32_ucomineq(tmp_V4f, tmp_V4f);
  tmp_i = __builtin_ia32_comisdeq(tmp_V2d, tmp_V2d);
  tmp_i = __builtin_ia32_comisdlt(tmp_V2d, tmp_V2d);
  tmp_i = __builtin_ia32_comisdle(tmp_V2d, tmp_V2d);
  tmp_i = __builtin_ia32_comisdgt(tmp_V2d, tmp_V2d);
  tmp_i = __builtin_ia32_comisdge(tmp_V2d, tmp_V2d);
  tmp_i = __builtin_ia32_comisdneq(tmp_V2d, tmp_V2d);
  tmp_i = __builtin_ia32_ucomisdeq(tmp_V2d, tmp_V2d);
  tmp_i = __builtin_ia32_ucomisdlt(tmp_V2d, tmp_V2d);
  tmp_i = __builtin_ia32_ucomisdle(tmp_V2d, tmp_V2d);
  tmp_i = __builtin_ia32_ucomisdgt(tmp_V2d, tmp_V2d);
  tmp_i = __builtin_ia32_ucomisdge(tmp_V2d, tmp_V2d);
  tmp_i = __builtin_ia32_ucomisdneq(tmp_V2d, tmp_V2d);
  tmp_V4f = __builtin_ia32_cmpps(tmp_V4f, tmp_V4f, 0);
  tmp_V4f = __builtin_ia32_cmpps(tmp_V4f, tmp_V4f, 1);
  tmp_V4f = __builtin_ia32_cmpps(tmp_V4f, tmp_V4f, 2);
  tmp_V4f = __builtin_ia32_cmpps(tmp_V4f, tmp_V4f, 3);
  tmp_V4f = __builtin_ia32_cmpps(tmp_V4f, tmp_V4f, 4);
  tmp_V4f = __builtin_ia32_cmpps(tmp_V4f, tmp_V4f, 5);
  tmp_V4f = __builtin_ia32_cmpps(tmp_V4f, tmp_V4f, 6);
  tmp_V4f = __builtin_ia32_cmpps(tmp_V4f, tmp_V4f, 7);
  tmp_V4f = __builtin_ia32_cmpss(tmp_V4f, tmp_V4f, 0);
  tmp_V4f = __builtin_ia32_cmpss(tmp_V4f, tmp_V4f, 1);
  tmp_V4f = __builtin_ia32_cmpss(tmp_V4f, tmp_V4f, 2);
  tmp_V4f = __builtin_ia32_cmpss(tmp_V4f, tmp_V4f, 3);
  tmp_V4f = __builtin_ia32_cmpss(tmp_V4f, tmp_V4f, 4);
  tmp_V4f = __builtin_ia32_cmpss(tmp_V4f, tmp_V4f, 5);
  tmp_V4f = __builtin_ia32_cmpss(tmp_V4f, tmp_V4f, 6);
  tmp_V4f = __builtin_ia32_cmpss(tmp_V4f, tmp_V4f, 7);
  tmp_V4f = __builtin_ia32_minps(tmp_V4f, tmp_V4f);
  tmp_V4f = __builtin_ia32_maxps(tmp_V4f, tmp_V4f);
  tmp_V4f = __builtin_ia32_minss(tmp_V4f, tmp_V4f);
  tmp_V4f = __builtin_ia32_maxss(tmp_V4f, tmp_V4f);

  tmp_V8c = __builtin_ia32_paddsb(tmp_V8c, tmp_V8c);
  tmp_V4s = __builtin_ia32_paddsw(tmp_V4s, tmp_V4s);
  tmp_V8c = __builtin_ia32_psubsb(tmp_V8c, tmp_V8c);
  tmp_V4s = __builtin_ia32_psubsw(tmp_V4s, tmp_V4s);
  tmp_V8c = __builtin_ia32_paddusb(tmp_V8c, tmp_V8c);
  tmp_V4s = __builtin_ia32_paddusw(tmp_V4s, tmp_V4s);
  tmp_V8c = __builtin_ia32_psubusb(tmp_V8c, tmp_V8c);
  tmp_V4s = __builtin_ia32_psubusw(tmp_V4s, tmp_V4s);
  tmp_V4s = __builtin_ia32_pmulhw(tmp_V4s, tmp_V4s);
  tmp_V4s = __builtin_ia32_pmulhuw(tmp_V4s, tmp_V4s);
  tmp_V8c = __builtin_ia32_pavgb(tmp_V8c, tmp_V8c);
  tmp_V4s = __builtin_ia32_pavgw(tmp_V4s, tmp_V4s);
  tmp_V8c = __builtin_ia32_pcmpeqb(tmp_V8c, tmp_V8c);
  tmp_V4s = __builtin_ia32_pcmpeqw(tmp_V4s, tmp_V4s);
  tmp_V2i = __builtin_ia32_pcmpeqd(tmp_V2i, tmp_V2i);
  tmp_V8c = __builtin_ia32_pcmpgtb(tmp_V8c, tmp_V8c);
  tmp_V4s = __builtin_ia32_pcmpgtw(tmp_V4s, tmp_V4s);
  tmp_V2i = __builtin_ia32_pcmpgtd(tmp_V2i, tmp_V2i);
  tmp_V8c = __builtin_ia32_pmaxub(tmp_V8c, tmp_V8c);
  tmp_V4s = __builtin_ia32_pmaxsw(tmp_V4s, tmp_V4s);
  tmp_V8c = __builtin_ia32_pminub(tmp_V8c, tmp_V8c);
  tmp_V4s = __builtin_ia32_pminsw(tmp_V4s, tmp_V4s);
  tmp_V2d = __builtin_ia32_cmppd(tmp_V2d, tmp_V2d, 0);
  tmp_V2d = __builtin_ia32_cmppd(tmp_V2d, tmp_V2d, 1);
  tmp_V2d = __builtin_ia32_cmppd(tmp_V2d, tmp_V2d, 2);
  tmp_V2d = __builtin_ia32_cmppd(tmp_V2d, tmp_V2d, 3);
  tmp_V2d = __builtin_ia32_cmppd(tmp_V2d, tmp_V2d, 4);
  tmp_V2d = __builtin_ia32_cmppd(tmp_V2d, tmp_V2d, 5);
  tmp_V2d = __builtin_ia32_cmppd(tmp_V2d, tmp_V2d, 6);
  tmp_V2d = __builtin_ia32_cmppd(tmp_V2d, tmp_V2d, 7);
  tmp_V2d = __builtin_ia32_cmpsd(tmp_V2d, tmp_V2d, 0);
  tmp_V2d = __builtin_ia32_cmpsd(tmp_V2d, tmp_V2d, 1);
  tmp_V2d = __builtin_ia32_cmpsd(tmp_V2d, tmp_V2d, 2);
  tmp_V2d = __builtin_ia32_cmpsd(tmp_V2d, tmp_V2d, 3);
  tmp_V2d = __builtin_ia32_cmpsd(tmp_V2d, tmp_V2d, 4);
  tmp_V2d = __builtin_ia32_cmpsd(tmp_V2d, tmp_V2d, 5);
  tmp_V2d = __builtin_ia32_cmpsd(tmp_V2d, tmp_V2d, 6);
  tmp_V2d = __builtin_ia32_cmpsd(tmp_V2d, tmp_V2d, 7);
  tmp_V2d = __builtin_ia32_minpd(tmp_V2d, tmp_V2d);
  tmp_V2d = __builtin_ia32_maxpd(tmp_V2d, tmp_V2d);
  tmp_V2d = __builtin_ia32_minsd(tmp_V2d, tmp_V2d);
  tmp_V2d = __builtin_ia32_maxsd(tmp_V2d, tmp_V2d);
  tmp_V16c = __builtin_ia32_paddsb128(tmp_V16c, tmp_V16c);
  tmp_V8s = __builtin_ia32_paddsw128(tmp_V8s, tmp_V8s);
  tmp_V16c = __builtin_ia32_psubsb128(tmp_V16c, tmp_V16c);
  tmp_V8s = __builtin_ia32_psubsw128(tmp_V8s, tmp_V8s);
  tmp_V16c = __builtin_ia32_paddusb128(tmp_V16c, tmp_V16c);
  tmp_V8s = __builtin_ia32_paddusw128(tmp_V8s, tmp_V8s);
  tmp_V16c = __builtin_ia32_psubusb128(tmp_V16c, tmp_V16c);
  tmp_V8s = __builtin_ia32_psubusw128(tmp_V8s, tmp_V8s);
  tmp_V8s = __builtin_ia32_pmulhw128(tmp_V8s, tmp_V8s);
  tmp_V16c = __builtin_ia32_pavgb128(tmp_V16c, tmp_V16c);
  tmp_V8s = __builtin_ia32_pavgw128(tmp_V8s, tmp_V8s);
  tmp_V16c = __builtin_ia32_pcmpeqb128(tmp_V16c, tmp_V16c);
  tmp_V8s = __builtin_ia32_pcmpeqw128(tmp_V8s, tmp_V8s);
  tmp_V4i = __builtin_ia32_pcmpeqd128(tmp_V4i, tmp_V4i);
  tmp_V16c = __builtin_ia32_pcmpgtb128(tmp_V16c, tmp_V16c);
  tmp_V8s = __builtin_ia32_pcmpgtw128(tmp_V8s, tmp_V8s);
  tmp_V4i = __builtin_ia32_pcmpgtd128(tmp_V4i, tmp_V4i);
  tmp_V16c = __builtin_ia32_pmaxub128(tmp_V16c, tmp_V16c);
  tmp_V8s = __builtin_ia32_pmaxsw128(tmp_V8s, tmp_V8s);
  tmp_V16c = __builtin_ia32_pminub128(tmp_V16c, tmp_V16c);
  tmp_V8s = __builtin_ia32_pminsw128(tmp_V8s, tmp_V8s);
  tmp_V8s = __builtin_ia32_packsswb128(tmp_V8s, tmp_V8s);
  tmp_V4i = __builtin_ia32_packssdw128(tmp_V4i, tmp_V4i);
  tmp_V8s = __builtin_ia32_packuswb128(tmp_V8s, tmp_V8s);
  tmp_V8s = __builtin_ia32_pmulhuw128(tmp_V8s, tmp_V8s);
  tmp_V4f = __builtin_ia32_addsubps(tmp_V4f, tmp_V4f);
  tmp_V2d = __builtin_ia32_addsubpd(tmp_V2d, tmp_V2d);
  tmp_V4f = __builtin_ia32_haddps(tmp_V4f, tmp_V4f);
  tmp_V2d = __builtin_ia32_haddpd(tmp_V2d, tmp_V2d);
  tmp_V4f = __builtin_ia32_hsubps(tmp_V4f, tmp_V4f);
  tmp_V2d = __builtin_ia32_hsubpd(tmp_V2d, tmp_V2d);
  tmp_V8s = __builtin_ia32_phaddw128(tmp_V8s, tmp_V8s);
  tmp_V4s = __builtin_ia32_phaddw(tmp_V4s, tmp_V4s);
  tmp_V4i = __builtin_ia32_phaddd128(tmp_V4i, tmp_V4i);
  tmp_V2i = __builtin_ia32_phaddd(tmp_V2i, tmp_V2i);
  tmp_V8s = __builtin_ia32_phaddsw128(tmp_V8s, tmp_V8s);
  tmp_V4s = __builtin_ia32_phaddsw(tmp_V4s, tmp_V4s);
  tmp_V8s = __builtin_ia32_phsubw128(tmp_V8s, tmp_V8s);
  tmp_V4s = __builtin_ia32_phsubw(tmp_V4s, tmp_V4s);
  tmp_V4i = __builtin_ia32_phsubd128(tmp_V4i, tmp_V4i);
  tmp_V2i = __builtin_ia32_phsubd(tmp_V2i, tmp_V2i);
  tmp_V8s = __builtin_ia32_phsubsw128(tmp_V8s, tmp_V8s);
  tmp_V4s = __builtin_ia32_phsubsw(tmp_V4s, tmp_V4s);
  tmp_V16c = __builtin_ia32_pmaddubsw128(tmp_V16c, tmp_V16c);
  tmp_V8c = __builtin_ia32_pmaddubsw(tmp_V8c, tmp_V8c);
  tmp_V8s = __builtin_ia32_pmulhrsw128(tmp_V8s, tmp_V8s);
  tmp_V4s = __builtin_ia32_pmulhrsw(tmp_V4s, tmp_V4s);
  tmp_V16c = __builtin_ia32_pshufb128(tmp_V16c, tmp_V16c);
  tmp_V8c = __builtin_ia32_pshufb(tmp_V8c, tmp_V8c);
  tmp_V16c = __builtin_ia32_psignb128(tmp_V16c, tmp_V16c);
  tmp_V8c = __builtin_ia32_psignb(tmp_V8c, tmp_V8c);
  tmp_V8s = __builtin_ia32_psignw128(tmp_V8s, tmp_V8s);
  tmp_V4s = __builtin_ia32_psignw(tmp_V4s, tmp_V4s);
  tmp_V4i = __builtin_ia32_psignd128(tmp_V4i, tmp_V4i);
  tmp_V2i = __builtin_ia32_psignd(tmp_V2i, tmp_V2i);
  tmp_V16c = __builtin_ia32_pabsb128(tmp_V16c);
  tmp_V8c = __builtin_ia32_pabsb(tmp_V8c);
  tmp_V8s = __builtin_ia32_pabsw128(tmp_V8s);
  tmp_V4s = __builtin_ia32_pabsw(tmp_V4s);
  tmp_V4i = __builtin_ia32_pabsd128(tmp_V4i);
  tmp_V2i = __builtin_ia32_pabsd(tmp_V2i);
  tmp_V4s = __builtin_ia32_psllw(tmp_V4s, tmp_V1LLi);
  tmp_V2i = __builtin_ia32_pslld(tmp_V2i, tmp_V1LLi);
  tmp_V1LLi = __builtin_ia32_psllq(tmp_V1LLi, tmp_V1LLi);
  tmp_V4s = __builtin_ia32_psrlw(tmp_V4s, tmp_V1LLi);
  tmp_V2i = __builtin_ia32_psrld(tmp_V2i, tmp_V1LLi);
  tmp_V1LLi = __builtin_ia32_psrlq(tmp_V1LLi, tmp_V1LLi);
  tmp_V4s = __builtin_ia32_psraw(tmp_V4s, tmp_V1LLi);
  tmp_V2i = __builtin_ia32_psrad(tmp_V2i, tmp_V1LLi);
  tmp_V2i = __builtin_ia32_pmaddwd(tmp_V4s, tmp_V4s);
  tmp_V8c = __builtin_ia32_packsswb(tmp_V4s, tmp_V4s);
  tmp_V4s = __builtin_ia32_packssdw(tmp_V2i, tmp_V2i);
  tmp_V8c = __builtin_ia32_packuswb(tmp_V4s, tmp_V4s);

  (void) __builtin_ia32_ldmxcsr(tmp_Ui);
  tmp_Ui = __builtin_ia32_stmxcsr();
  tmp_V4f = __builtin_ia32_cvtpi2ps(tmp_V4f, tmp_V2i);
  tmp_V2i = __builtin_ia32_cvtps2pi(tmp_V4f);
  tmp_i = __builtin_ia32_cvtss2si(tmp_V4f);
#ifdef USE_64
  tmp_LLi = __builtin_ia32_cvtss2si64(tmp_V4f);
#endif
  tmp_V2i = __builtin_ia32_cvttps2pi(tmp_V4f);
  (void) __builtin_ia32_maskmovq(tmp_V8c, tmp_V8c, tmp_cp);
  tmp_V4f = __builtin_ia32_loadups(tmp_fCp);
  (void) __builtin_ia32_storeups(tmp_fp, tmp_V4f);
  (void) __builtin_ia32_storehps(tmp_V2ip, tmp_V4f);
  (void) __builtin_ia32_storelps(tmp_V2ip, tmp_V4f);
  tmp_i = __builtin_ia32_movmskps(tmp_V4f);
  tmp_i = __builtin_ia32_pmovmskb(tmp_V8c);
  (void) __builtin_ia32_movntps(tmp_fp, tmp_V4f);
  (void) __builtin_ia32_movntq(tmp_V1LLip, tmp_V1LLi);
  (void) __builtin_ia32_sfence();

  tmp_V4s = __builtin_ia32_psadbw(tmp_V8c, tmp_V8c);
  tmp_V4f = __builtin_ia32_rcpps(tmp_V4f);
  tmp_V4f = __builtin_ia32_rcpss(tmp_V4f);
  tmp_V4f = __builtin_ia32_rsqrtps(tmp_V4f);
  tmp_V4f = __builtin_ia32_rsqrtss(tmp_V4f);
  tmp_V4f = __builtin_ia32_sqrtps(tmp_V4f);
  tmp_V4f = __builtin_ia32_sqrtss(tmp_V4f);
  (void) __builtin_ia32_maskmovdqu(tmp_V16c, tmp_V16c, tmp_cp);
  tmp_V2d = __builtin_ia32_loadupd(tmp_dCp);
  (void) __builtin_ia32_storeupd(tmp_dp, tmp_V2d);
  tmp_i = __builtin_ia32_movmskpd(tmp_V2d);
  tmp_i = __builtin_ia32_pmovmskb128(tmp_V16c);
  (void) __builtin_ia32_movnti(tmp_ip, tmp_i);
  (void) __builtin_ia32_movntpd(tmp_dp, tmp_V2d);
  (void) __builtin_ia32_movntdq(tmp_V2LLip, tmp_V2LLi);
  tmp_V2LLi = __builtin_ia32_psadbw128(tmp_V16c, tmp_V16c);
  tmp_V2d = __builtin_ia32_sqrtpd(tmp_V2d);
  tmp_V2d = __builtin_ia32_sqrtsd(tmp_V2d);
  tmp_V2d = __builtin_ia32_cvtdq2pd(tmp_V4i);
  tmp_V4f = __builtin_ia32_cvtdq2ps(tmp_V4i);
  tmp_V2LLi = __builtin_ia32_cvtpd2dq(tmp_V2d);
  tmp_V2i = __builtin_ia32_cvtpd2pi(tmp_V2d);
  tmp_V4f = __builtin_ia32_cvtpd2ps(tmp_V2d);
  tmp_V4i = __builtin_ia32_cvttpd2dq(tmp_V2d);
  tmp_V2i = __builtin_ia32_cvttpd2pi(tmp_V2d);
  tmp_V2d = __builtin_ia32_cvtpi2pd(tmp_V2i);
  tmp_i = __builtin_ia32_cvtsd2si(tmp_V2d);
#ifdef USE_64
  tmp_LLi = __builtin_ia32_cvtsd2si64(tmp_V2d);
#endif
  tmp_V4i = __builtin_ia32_cvtps2dq(tmp_V4f);
  tmp_V2d = __builtin_ia32_cvtps2pd(tmp_V4f);
  tmp_V4i = __builtin_ia32_cvttps2dq(tmp_V4f);
  (void) __builtin_ia32_clflush(tmp_vCp);
  (void) __builtin_ia32_lfence();
  (void) __builtin_ia32_mfence();
  tmp_V16c = __builtin_ia32_loaddqu(tmp_cCp);
  (void) __builtin_ia32_storedqu(tmp_cp, tmp_V16c);
  tmp_V4s = __builtin_ia32_psllwi(tmp_V4s, tmp_i);
  tmp_V2i = __builtin_ia32_pslldi(tmp_V2i, tmp_i);
  tmp_V1LLi = __builtin_ia32_psllqi(tmp_V1LLi, tmp_i);
  tmp_V4s = __builtin_ia32_psrawi(tmp_V4s, tmp_i);
  tmp_V2i = __builtin_ia32_psradi(tmp_V2i, tmp_i);
  tmp_V4s = __builtin_ia32_psrlwi(tmp_V4s, tmp_i);
  tmp_V2i = __builtin_ia32_psrldi(tmp_V2i, tmp_i);
  tmp_V1LLi = __builtin_ia32_psrlqi(tmp_V1LLi, tmp_i);
  tmp_V1LLi = __builtin_ia32_pmuludq(tmp_V2i, tmp_V2i);
  tmp_V2LLi = __builtin_ia32_pmuludq128(tmp_V4i, tmp_V4i);
  tmp_V8s = __builtin_ia32_psraw128(tmp_V8s, tmp_V8s);
  tmp_V4i = __builtin_ia32_psrad128(tmp_V4i, tmp_V4i);
  tmp_V8s = __builtin_ia32_psrlw128(tmp_V8s, tmp_V8s);
  tmp_V4i = __builtin_ia32_psrld128(tmp_V4i, tmp_V4i);
  tmp_V2LLi = __builtin_ia32_psrlq128(tmp_V2LLi, tmp_V2LLi);
  tmp_V8s = __builtin_ia32_psllw128(tmp_V8s, tmp_V8s);
  tmp_V4i = __builtin_ia32_pslld128(tmp_V4i, tmp_V4i);
  tmp_V2LLi = __builtin_ia32_psllq128(tmp_V2LLi, tmp_V2LLi);
  tmp_V8s = __builtin_ia32_psllwi128(tmp_V8s, tmp_i);
  tmp_V4i = __builtin_ia32_pslldi128(tmp_V4i, tmp_i);
  tmp_V2LLi = __builtin_ia32_psllqi128(tmp_V2LLi, tmp_i);
  tmp_V8s = __builtin_ia32_psrlwi128(tmp_V8s, tmp_i);
  tmp_V4i = __builtin_ia32_psrldi128(tmp_V4i, tmp_i);
  tmp_V2LLi = __builtin_ia32_psrlqi128(tmp_V2LLi, tmp_i);
  tmp_V8s = __builtin_ia32_psrawi128(tmp_V8s, tmp_i);
  tmp_V4i = __builtin_ia32_psradi128(tmp_V4i, tmp_i);
  tmp_V8s = __builtin_ia32_pmaddwd128(tmp_V8s, tmp_V8s);
  (void) __builtin_ia32_monitor(tmp_vp, tmp_Ui, tmp_Ui);
  (void) __builtin_ia32_mwait(tmp_Ui, tmp_Ui);
  tmp_V16c = __builtin_ia32_lddqu(tmp_cCp);
  tmp_V2LLi = __builtin_ia32_palignr128(tmp_V2LLi, tmp_V2LLi, imm_i);
  tmp_V1LLi = __builtin_ia32_palignr(tmp_V1LLi, tmp_V1LLi, imm_i);
  (void) __builtin_ia32_storelv4si(tmp_V2ip, tmp_V2LLi);
#ifdef USE_SSE4
  tmp_V16c = __builtin_ia32_pblendvb128(tmp_V16c, tmp_V16c, tmp_V16c);
  tmp_V8s = __builtin_ia32_pblendw128(tmp_V8s, tmp_V8s, imm_i_0_256);
  tmp_V2d = __builtin_ia32_blendpd(tmp_V2d, tmp_V2d, imm_i_0_256);
  tmp_V4f = __builtin_ia32_blendps(tmp_V4f, tmp_V4f, imm_i_0_256);
  tmp_V2d = __builtin_ia32_blendvpd(tmp_V2d, tmp_V2d, tmp_V2d);
  tmp_V4f = __builtin_ia32_blendvps(tmp_V4f, tmp_V4f, tmp_V4f);
  tmp_V8s = __builtin_ia32_packusdw128(tmp_V4i, tmp_V4i);
  tmp_V16c = __builtin_ia32_pmaxsb128(tmp_V16c, tmp_V16c);
  tmp_V4i = __builtin_ia32_pmaxsd128(tmp_V4i, tmp_V4i);
  tmp_V4i = __builtin_ia32_pmaxud128(tmp_V4i, tmp_V4i);
  tmp_V8s = __builtin_ia32_pmaxuw128(tmp_V8s, tmp_V8s);
  tmp_V16c = __builtin_ia32_pminsb128(tmp_V16c, tmp_V16c);
  tmp_V4i = __builtin_ia32_pminsd128(tmp_V4i, tmp_V4i);
  tmp_V4i = __builtin_ia32_pminud128(tmp_V4i, tmp_V4i);
  tmp_V8s = __builtin_ia32_pminuw128(tmp_V8s, tmp_V8s);
  tmp_V4i = __builtin_ia32_pmovsxbd128(tmp_V16c);
  tmp_V2LLi = __builtin_ia32_pmovsxbq128(tmp_V16c);
  tmp_V8s = __builtin_ia32_pmovsxbw128(tmp_V16c);
  tmp_V2LLi = __builtin_ia32_pmovsxdq128(tmp_V4i);
  tmp_V4i = __builtin_ia32_pmovsxwd128(tmp_V8s);
  tmp_V2LLi = __builtin_ia32_pmovsxwq128(tmp_V8s);
  tmp_V4i = __builtin_ia32_pmovzxbd128(tmp_V16c);
  tmp_V2LLi = __builtin_ia32_pmovzxbq128(tmp_V16c);
  tmp_V8s = __builtin_ia32_pmovzxbw128(tmp_V16c);
  tmp_V2LLi = __builtin_ia32_pmovzxdq128(tmp_V4i);
  tmp_V4i = __builtin_ia32_pmovzxwd128(tmp_V8s);
  tmp_V2LLi = __builtin_ia32_pmovzxwq128(tmp_V8s);
  tmp_V2LLi = __builtin_ia32_pmuldq128(tmp_V4i, tmp_V4i);
  tmp_V4i = __builtin_ia32_pmulld128(tmp_V4i, tmp_V4i);
  tmp_V4f = __builtin_ia32_roundps(tmp_V4f, imm_i_0_16);
  //  tmp_V4f = __builtin_ia32_roundss(tmp_V4f, tmp_V4f, imm_i_0_16);
  //  tmp_V2d = __builtin_ia32_roundsd(tmp_V2d, tmp_V2d, imm_i_0_16);
  tmp_V2d = __builtin_ia32_roundpd(tmp_V2d, imm_i_0_16);
  tmp_V4f = __builtin_ia32_insertps128(tmp_V4f, tmp_V4f, tmp_i);
#endif
}


