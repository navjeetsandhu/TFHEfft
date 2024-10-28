#pragma once

#include <array>
#include <cstdint>

#include "mulfft.hpp"
#include "params.hpp"
#include "trlwe.hpp"

namespace TFHEpp {

template <class P>
constexpr typename P::T offsetgen()
{
    typename P::T offset = 0;
    for (int i = 1; i <= P::l; i++)
        offset += P::Bg / 2 *
                  (1ULL << (std::numeric_limits<typename P::T>::digits -
                            i * P::Bgbit));
    return offset;
}

template <class P>
inline void Decomposition(DecomposedPolynomial<P> &decpoly,
                          const Polynomial<P> &poly, typename P::T randbits = 0)
{
    constexpr typename P::T offset = offsetgen<P>();
    constexpr typename P::T roundoffset =
        1ULL << (std::numeric_limits<typename P::T>::digits - P::l * P::Bgbit -
                 1);
    constexpr typename P::T mask =
        static_cast<typename P::T>((1ULL << P::Bgbit) - 1);
    constexpr typename P::T halfBg = (1ULL << (P::Bgbit - 1));

    for (int i = 0; i < P::n; i++) {
        for (int l = 0; l < P::l; l++)
            decpoly[l][i] = (((poly[i] + offset + roundoffset) >>
                              (std::numeric_limits<typename P::T>::digits -
                               (l + 1) * P::Bgbit)) &
                             mask) -
                            halfBg;
    }
}

template <class P>
void DecompositionNTT(DecomposedPolynomialNTT<P> &decpolyntt,
                      const Polynomial<P> &poly, typename P::T randbits = 0)
{
    DecomposedPolynomial<P> decpoly;
    Decomposition<P>(decpoly, poly);
    for (int i = 0; i < P::l; i++) TwistINTT<P>(decpolyntt[i], decpoly[i]);
}

template <class P>
void DecompositionRAINTT(DecomposedPolynomialRAINTT<P> &decpolyntt,
                         const Polynomial<P> &poly, typename P::T randbits = 0)
{
    DecomposedPolynomial<P> decpoly;
    Decomposition<P>(decpoly, poly);
    for (int i = 0; i < P::l; i++)
        raintt::TwistINTT<typename P::T, P::nbit, false>(
            decpolyntt[i], decpoly[i], (*raintttable)[1], (*raintttwist)[1]);
}


template <class P>
constexpr std::array<typename P::T, P::l> hgen()
{
    std::array<typename P::T, P::l> h{};
    if constexpr (hasq<P>::value)
        for (int i = 0; i < P::l; i++)
            h[i] = (P::q + (1ULL << ((i + 1) * P::Bgbit - 1))) >>
                   ((i + 1) * P::Bgbit);
    else
        for (int i = 0; i < P::l; i++)
            h[i] = 1ULL << (std::numeric_limits<typename P::T>::digits -
                            (i + 1) * P::Bgbit);
    return h;
}

template <class P>
TRGSWFFT<P> ApplyFFT2trgsw(const TRGSW<P> &trgsw)
{
    alignas(64) TRGSWFFT<P> trgswfft;
    for (int i = 0; i < (P::k + 1) * P::l; i++)
        for (int j = 0; j < (P::k + 1); j++)
            TwistIFFT<P>(trgswfft[i][j], trgsw[i][j]);
    return trgswfft;
}

template <class P>
TRGSWNTT<P> ApplyNTT2trgsw(const TRGSW<P> &trgsw)
{
    TRGSWNTT<P> trgswntt;
    for (int i = 0; i < (P::k + 1) * P::l; i++)
        for (int j = 0; j < P::k + 1; j++)
            TwistINTT<P>(trgswntt[i][j], trgsw[i][j]);
    return trgswntt;
}

template <class P>
TRGSWRAINTT<P> ApplyRAINTT2trgsw(const TRGSW<P> &trgsw)
{
    constexpr uint8_t remainder = ((P::nbit - 1) % 3) + 1;
    TRGSWRAINTT<P> trgswntt;
    for (int i = 0; i < (P::k + 1) * P::l; i++)
        for (int j = 0; j < P::k + 1; j++) {
            raintt::TwistINTT<typename P::T, P::nbit, true>(
                trgswntt[i][j], trgsw[i][j], (*raintttable)[1],
                (*raintttwist)[1]);
            for (int k = 0; k < P::n; k++)
                if ((k & ((1 << remainder) - 1)) > 1)
                    trgswntt[i][j][k] =
                        raintt::MulSREDC(trgswntt[i][j][k], raintt::R4);
                else
                    trgswntt[i][j][k] =
                        raintt::MulSREDC(trgswntt[i][j][k], raintt::R2);
        }
    return trgswntt;
}

template <class P>
TRGSWNTT<P> TRGSW2NTT(const TRGSW<P> &trgsw)
{
    TRGSWNTT<P> trgswntt;
    for (int i = 0; i < (P::k + 1) * P::l; i++)
        for (int j = 0; j < P::k + 1; j++) {
            PolynomialNTT<P> temp;
            TwistINTT<P>(temp, trgsw[i][j]);
            for (uint32_t k = 0; k < P::n; k++)
                trgswntt[i][j][k] = temp[cuHEpp::BitReverse<P::nbit>(k)];
        }
    return trgswntt;
}

template <class P>
TRGSW<P> trgswSymEncrypt(const Polynomial<P> &p, const double alpha,
                         const Key<P> &key)
{
    constexpr std::array<typename P::T, P::l> h = hgen<P>();

    TRGSW<P> trgsw;
    for (TRLWE<P> &trlwe : trgsw) trlwe = trlweSymEncryptZero<P>(alpha, key);
    for (int i = 0; i < P::l; i++) {
        for (int k = 0; k < P::k + 1; k++) {
            for (int j = 0; j < P::n; j++) {
                trgsw[i + k * P::l][k][j] +=
                    static_cast<typename P::T>(p[j]) * h[i];
            }
        }
    }
    return trgsw;
}

template <class P>
TRGSW<P> trgswSymEncrypt(const Polynomial<P> &p, const uint eta,
                         const Key<P> &key)
{
    constexpr std::array<typename P::T, P::l> h = hgen<P>();

    TRGSW<P> trgsw;
    for (TRLWE<P> &trlwe : trgsw) trlwe = trlweSymEncryptZero<P>(eta, key);
    for (int i = 0; i < P::l; i++) {
        for (int k = 0; k < P::k + 1; k++) {
            for (int j = 0; j < P::n; j++) {
                trgsw[i + k * P::l][k][j] +=
                    static_cast<typename P::T>(p[j]) * h[i];
            }
        }
    }
    return trgsw;
}

template <class P>
TRGSW<P> trgswSymEncrypt(const Polynomial<P> &p, const Key<P> &key)
{
    if constexpr (P::errordist == ErrorDistribution::ModularGaussian)
        return trgswSymEncrypt<P>(p, P::alpha, key);
    else
        return trgswSymEncrypt<P>(p, P::eta, key);
}

template <class P>
TRGSWFFT<P> trgswfftSymEncrypt(const Polynomial<P> &p, const double alpha,
                               const Key<P> &key)
{
    TRGSW<P> trgsw = trgswSymEncrypt<P>(p, alpha, key);
    return ApplyFFT2trgsw<P>(trgsw);
}

template <class P>
TRGSWFFT<P> trgswfftSymEncrypt(const Polynomial<P> &p, const uint eta,
                               const Key<P> &key)
{
    TRGSW<P> trgsw = trgswSymEncrypt<P>(p, eta, key);
    return ApplyFFT2trgsw<P>(trgsw);
}

template <class P>
TRGSWFFT<P> trgswfftSymEncrypt(const Polynomial<P> &p, const Key<P> &key)
{
    if constexpr (P::errordist == ErrorDistribution::ModularGaussian)
        return trgswfftSymEncrypt<P>(p, P::alpha, key);
    else
        return trgswfftSymEncrypt<P>(p, P::eta, key);
}

template <class P>
TRGSWNTT<P> trgswnttSymEncrypt(const Polynomial<P> &p, const double alpha,
                               const Key<P> &key)
{
    TRGSW<P> trgsw = trgswSymEncrypt<P>(p, alpha, key);
    return ApplyNTT2trgsw<P>(trgsw);
}

template <class P>
TRGSWNTT<P> trgswnttSymEncrypt(const Polynomial<P> &p, const uint eta,
                               const Key<P> &key)
{
    TRGSW<P> trgsw = trgswSymEncrypt<P>(p, eta, key);
    return ApplyNTT2trgsw<P>(trgsw);
}

template <class P>
TRGSWNTT<P> trgswnttSymEncrypt(const Polynomial<P> &p, const Key<P> &key)
{
    if constexpr (P::errordist == ErrorDistribution::ModularGaussian)
        return trgswnttSymEncrypt<P>(p, P::alpha, key);
    else
        return trgswnttSymEncrypt<P>(p, P::eta, key);
}

template <class P>
TRGSWRAINTT<P> trgswrainttSymEncrypt(const Polynomial<P> &p, const double alpha,
                                     const Key<P> &key)
{
    TRGSW<P> trgsw = trgswSymEncrypt<P>(p, alpha, key);
    return ApplyRAINTT2trgsw<P>(trgsw);
}

template <class P>
TRGSWRAINTT<P> trgswrainttSymEncrypt(const Polynomial<P> &p, const uint eta,
                                     const Key<P> &key)
{
    if constexpr (hasq<P>::value && P::q == raintt::P) {
        constexpr uint8_t remainder = ((P::nbit - 1) % 3) + 1;
        constexpr std::array<typename P::T, P::l> h = hgen<P>();
        TRGSWRAINTT<P> trgswraintt;
        for (TRLWERAINTT<P> &trlweraintt : trgswraintt) {
            trlweraintt = trlwerainttSymEncryptZero<P>(eta, key);
            for (int k = 0; k <= P::k; k++)
                for (int j = 0; j < P::n; j++)
                    trlweraintt[k][j] = raintt::MulSREDC(
                        trlweraintt[k][j], ((j & ((1 << remainder) - 1)) > 1)
                                               ? raintt::R3
                                               : raintt::R2);
        }
        for (int i = 0; i < P::l; i++) {
            Polynomial<P> pscaled;
            for (int j = 0; j < P::n; j++) {
                pscaled[j] =
                    static_cast<uint64_t>(static_cast<int32_t>(p[j]) >= 0
                                              ? p[j]
                                              : (P::q + p[j])) *
                    h[i] % P::q;
            }
            for (int k = 0; k < P::k + 1; k++) {
                PolynomialRAINTT<P> praintt;
                raintt::TwistINTT<typename P::T, P::nbit, false>(
                    praintt, pscaled, (*raintttable)[1], (*raintttwist)[1]);
                for (int j = 0; j < P::n; j++)
                    if ((j & ((1 << remainder) - 1)) > 1)
                        praintt[j] = raintt::MulSREDC(praintt[j], raintt::R4);
                    else
                        praintt[j] = raintt::MulSREDC(praintt[j], raintt::R2);
                for (int j = 0; j < P::n; j++)
                    trgswraintt[i + k * P::l][k][j] = raintt::AddMod(
                        trgswraintt[i + k * P::l][k][j], praintt[j]);
            }
        }
        return trgswraintt;
    }
    else {
        TRGSW<P> trgsw = trgswSymEncrypt<P>(p, eta, key);
        return ApplyRAINTT2trgsw<P>(trgsw);
    }
}

template <class P>
TRGSWRAINTT<P> trgswrainttSymEncrypt(const Polynomial<P> &p, const Key<P> &key)
{
    if constexpr (P::errordist == ErrorDistribution::ModularGaussian)
        return trgswrainttSymEncrypt<P>(p, P::alpha, key);
    else
        return trgswrainttSymEncrypt<P>(p, P::eta, key);
}

}  // namespace TFHEpp