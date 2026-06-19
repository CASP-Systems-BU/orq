#pragma once

#include <NTL/vec_ZZ.h>

#include "backend/common/libote_io.h"
#include "core/containers/encoding.h"
#include "core/math/math.h"
#include "gilboa_mod_p.h"
#include "gilboa_ole.h"
#include "ole_generator.h"

/**
 * @brief Statistical security parameter \f$\lambda\f$, with failure probability \f$2^-\lambda\f$
 *
 */
constexpr int STATISTICAL_SECURITY_PR = 40;

namespace orq::random {

namespace internal {
    // Type for primes & small OLEs
    using S = uint8_t;

    using base_mod_ptr = std::shared_ptr<OLEmodPrimeInterface<S>>;
    using namespace NTL;

    /**
     * @brief Helper class to run multiple CRT executions with the same setup.
     *
     */
    class CRTbasis {
        ZZ prodPrimes = ZZ(1);
        vec_ZZ coeffs;

       public:
        CRTbasis() {}
        CRTbasis(int upTo) {
            auto& P = orq::math::primes::PRIMES_WITHIN_BYTE;
            for (int i = 0; i < upTo; i++) {
                prodPrimes *= P[i];
            }

            coeffs.SetLength(upTo);

            // Compute the CRT coefficients
            for (int i = 0; i < upTo; i++) {
                ZZ Pi(P[i]);
                ZZ Mi = prodPrimes / Pi;
                ZZ inv = InvMod(Mi % Pi, Pi);
                coeffs[i] = Mi * inv;
            }
        }

        ZZ apply(const vec_ZZ& residues) const {
            // compute inner product and reduce
            // TODO: may be more efficient to do a loop and incremental mod reduce!
            auto x = (residues * coeffs) % prodPrimes;

            // Convert back to signed
            x -= prodPrimes;

            return x;
        }
    };

    /**
     * @brief Perfectly secure OLE over smooth integers.
     *
     * Protocol 4.5 of Doerner et al.
     *
     */
    class GilboaCRT_smooth {
        int numPrimes;
        base_mod_ptr base;

        CRTbasis basis;

       public:
        GilboaCRT_smooth(int numPrimes, base_mod_ptr b)
            : numPrimes(numPrimes), basis(numPrimes), base(b) {
            assert(0 <= numPrimes && numPrimes <= 54);

            int numOTs = 0;

            for (int i = 0; i < numPrimes; i++) {
                numOTs += std::bit_width(uint8_t(orq::math::primes::PRIMES_WITHIN_BYTE[i] - 1));
            }

            // TODO: pool required OTs (make_pooled(b)? so that we don't pay setup costs multiple
            // times)
        }

        vec_ZZ getNext(vec_ZZ a, std::optional<NTL::vec_ZZ> b) {
            auto n = a.length();
            std::vector<NTL::vec_ZZ> v(n);

            if (b && b->length() != n) {
                throw std::logic_error("expected |b|=" + std::to_string(n) + " but got " +
                                       std::to_string(b->length()));
            }

            for (int j = 0; j < n; j++) {
                v[j].SetLength(numPrimes);
            }

            Vector<S> aa(n);
            std::optional<Vector<S>> bb;
            if (b) {
                bb = std::make_optional<Vector<S>>(n);
            }

            // This could be in parallel, but we're parallelizing at the vector level for now
            for (int i = 0; i < numPrimes; i++) {
                int pi = orq::math::primes::PRIMES_WITHIN_BYTE[i];
                base->setPrime(pi);

                // Reduce the additive share mod p
                if (b) {
                    for (int j = 0; j < n; j++) {
                        (*bb)[j] = (*b)[j] % pi;
                    }
                }
                // else: bb was already nullopt.

                // Reduce the multiplicative share mod p
                for (int j = 0; j < n; j++) {
                    aa[j] = a[j] % pi;
                }

                // Get a chosen-message OLE mod p
                auto w = base->getNext(aa, bb);

                // copy the i-th column back over
                for (int j = 0; j < n; j++) {
                    v[j][i] = w[j];
                }
            }

            // Sender returns immediately
            if (b) {
                return *b;
            }

            // Receiver does CRT reconstruction
            vec_ZZ x;
            x.SetLength(n);

            for (int j = 0; j < n; j++) {
                x[j] = basis.apply(v[j]);
            }

            return x;
        }
    };
}  // namespace internal

/**
 * @brief Gilboa OLE generator using CRT protocol of Doerner et al.
 *
 * Paper: https://eprint.iacr.org/2025/1722
 *
 * We implement the semihonest protocol (Protocol 4.7) below, and call down to the smooth-integer
 * version (Protocol 4.5)
 *
 * Underlying OLEs are currently fixed to 8 bits. This means that, while the protocol works for
 * `T=int8_t`, it is advisable to use the base quadratic protocol for concrete efficiency.
 *
 * @tparam T
 */
template <typename T>
class GilboaCRT : public OLEGenerator<T> {
    static constexpr int L = std::numeric_limits<std::make_unsigned_t<T>>::digits;

    using OLEBase = OLEGenerator<T>;

    // 2^L
    NTL::ZZ Q = NTL::ZZ(1) << L;

    NTL::ZZ smoothMod;
    NTL::ZZ samplingBound;

    int maxPrimeIdx;

    // The smooth OLE generator.
    std::unique_ptr<internal::GilboaCRT_smooth> Gs;

   public:
    GilboaCRT(internal::base_mod_ptr base, std::shared_ptr<CommonPRGManager> m)
        : OLEBase(base->getRank(), m, base->getComm()) {
        // Find a prime such that:
        // smoothMod > q^2 * 2^lambda
        std::tie(smoothMod, maxPrimeIdx) =
            orq::math::primes::primorial_gt_bits(2 * L + STATISTICAL_SECURITY_PR);

        // Sanity check
        assert(smoothMod > Q * Q);

        // We need to sample b' from [0, n - q^2] s.t. b' == b mod q.
        // Therefore, b' = b + qk and b + Qk <= n - q^2 (clearly k is at least 0)
        //
        // Then, since b < q, we have
        //   qk <= n - q^2 - b
        //    k <= (n - q^2) / q - 1
        // since k is an integer. This is an *inclusive* bound, so we add 1 below.
        samplingBound = (smoothMod - Q * Q) / Q - 1;

        Gs = std::make_unique<internal::GilboaCRT_smooth>(maxPrimeIdx + 1, base);
    }

    /**
     * @brief Implicit: default random input version.
     *
     */
    using OLEGenerator<T>::getNext;

    /**
     * @brief Generate an OLE correlation using the CRT protocol
     *
     * @param a multiplicative share
     * @param b sender's additive share
     * @return Vector<T>
     */
    Vector<T> getNext(Vector<T>& a, std::optional<Vector<T>> b) override {
        // Both parties generate and randomize a multiplicative share. We will
        // compute additive shares y(1) = a(0) * x(1) - b(0)

        auto n = a.size();
        std::optional<NTL::vec_ZZ> bp;

        if (b) {
            // Party 0 only
            bp = std::make_optional<NTL::vec_ZZ>();
            bp->SetLength(n);
            NTL::ZZ k;
            // TODO: VectorRandomBnd?
            for (size_t j = 0; j < n; j++) {
                // inclusive on both sides, so + 1.
                RandomBnd(k, samplingBound + 1);
                (*bp)[j] = orq::math::to_ZZ((*b)[j]) + Q * k;
            }
        }

        // Both parties copy their multiplicative shares into a vec_ZZ.
        NTL::vec_ZZ aa;
        aa.SetLength(n);
        for (int j = 0; j < n; j++) {
            aa[j] = orq::math::to_ZZ(a[j]);
        }

        // P1 passes an empty optional for its additive share `bp`.
        auto v = Gs->getNext(aa, bp);

        // P0 can return once the smooth OLE is done.
        if (b) {
            return *b;
        }

        // Only P1 has to reduce back to a Vector<>.
        Vector<T> add(n);
        for (size_t i = 0; i < n; i++) {
            // Implicit (mod Q) here, but since Q is a power of 2, type conversion handles it.
            if (v[i] < 0) v[i] = (v[i] % Q) + Q;
            add[i] = orq::math::from_ZZ<T>(v[i]);
        }

        stopwatch::timepoint("final");

        return add;
    }
};

}  // namespace orq::random