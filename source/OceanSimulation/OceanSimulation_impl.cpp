#include "OceanSimulation_decl.h"
#include "c4d.h"

#include "maxon/matrix_nxm.h"
#include "maxon/fft.h"
#include "maxon/parallelfor.h"
#include "maxon/atomictypes.h"
#include "maxon/jobgroup.h"
#include "maxon/parallelimage.h"
#include "c4d_thread.h"
#include <limits>
#include <type_traits>

static_assert(std::is_same_v<maxon::details::ComponentIdentifierMetaData<std::decay<decltype(OceanSimulation::Ocean)>::type, maxon::Class<>>::type, maxon::Class<OceanSimulation::OceanRef>>, "Ocean declaration metadata type mismatch");

namespace OceanSimulation
{
	const maxon::Float gravity = 9.81;

	static maxon::Float RandomGaussian(maxon::LinearCongruentialRandom<maxon::Float32>& random)
	{
		maxon::Float x, y, lengthsqr;
		do {
			x = random.Get11();
			y = random.Get11();
			lengthsqr = x * x + y * y;
		} while (lengthsqr >= 1.0 || lengthsqr == 0.0);

		return x * maxon::Sqrt(-2.0_f * maxon::Log2(lengthsqr) / lengthsqr);
	}

	template<typename T>
	static inline T const LinearInterpolation(const T a, const T b, const T f)
	{
		return a + (b - a) * f;
	}

	template<typename T>
	static inline T CatmullRomInterpolation(T p0, T p1, T p2, T p3, T f)
	{
		return 0.5 * ((2 * p1) + (-p0 + p2) * f + (2 * p0 - 5 * p1 + 4 * p2 - p3) * f * f + (-p0 + 3 * p1 - 3 * p2 + p3) * f * f * f);
	}

	class OceanImplementation : public maxon::Component<OceanImplementation, OceanInterface>
	{
		MAXON_COMPONENT();

	public:
		static maxon::Result<void> ConfigureClass(maxon::ClassInterface& cls)
		{
			return maxon::OK;
		}

		static maxon::Result<void> InitImplementation()
		{
			return maxon::OK;
		}

		maxon::Result<void> InitComponent()
		{
			return maxon::OK;
		}

		MAXON_METHOD maxon::Result<void> Init(maxon::Int32 oceanResolution, maxon::Float oceanSize, maxon::Float shortestWaveLength,
			maxon::Float amplitude, maxon::Float windSpeed, maxon::Float windDirection, maxon::Float alignement, maxon::Float damp,
			maxon::Int32 seed)
		{
			iferr_scope;


			M_ = N_ = oceanResolution;
			Lx_ = Lz_ = oceanSize;
			l_ = shortestWaveLength;
			A_ = amplitude;
			V_ = windSpeed;
			alignement_ = alignement;
			dampReflect_ = damp;
			seed_ = seed;

			kv_.Resize(M_, N_) iferr_return;
			km_.Resize(M_, N_) iferr_return;
			h0_.Resize(M_, N_) iferr_return;
			h0_minus_.Resize(M_, N_) iferr_return;
			htilda_.Resize(M_, N_) iferr_return;
			fftin_.Resize(M_, N_) iferr_return;
			foam_.Resize(M_, N_) iferr_return;
			jMinus_.Resize(M_, N_) iferr_return;
			dispX_.Resize(M_, N_) iferr_return;
			dispY_.Resize(M_, N_) iferr_return;
			dispZ_.Resize(M_, N_) iferr_return;
			normX_.Resize(M_, N_) iferr_return;
			normY_.Resize(M_, N_) iferr_return;
			normZ_.Resize(M_, N_) iferr_return;
			jxx_.Resize(M_, N_) iferr_return;
			jzz_.Resize(M_, N_) iferr_return;
			jxz_.Resize(M_, N_) iferr_return;

			maxon::LinearCongruentialRandom<maxon::Float32> randNumber;
			randNumber.Init(seed);

			WHat_ = GetWindDirectionNormalized(windDirection);

			for (maxon::Int32 i = 0, m_ = -M_ / 2; i < M_; i++, m_++)
			{
				for (maxon::Int32 j = 0, n_ = -N_ / 2; j < N_; j++, n_++)
				{
					maxon::Vector2d k(maxon::PI2 * m_ / Lx_, maxon::PI2 * n_ / Lz_);
					kv_(i, j) = k;
					km_(i, j) = k.GetLength();

					maxon::Complex<maxon::Float> rc(RandomGaussian(randNumber), RandomGaussian(randNumber));
					h0_(i, j) = rc * (maxon::Sqrt(Phillips(k)) * maxon::SQRT2_INV);
					h0_minus_(i, j) = rc * (maxon::Sqrt(Phillips(-k)) * maxon::SQRT2_INV);
				}
			}

			scaleNorm_ = CalculateNormalizedScale();
			return maxon::OK;
		}

		MAXON_METHOD maxon::Bool NeedUpdate(const maxon::Int32 oceanResolution, const maxon::Float oceanSize, const maxon::Float shortestWaveLength,
			const maxon::Float amplitude, const maxon::Float windSpeed, const maxon::Float windDirection, const maxon::Float alignement, const maxon::Float damp,
			const maxon::Int32 seed) const
		{
			if (M_ != oceanResolution || N_ != oceanResolution)
				return true;
			if (Lx_ != oceanSize || Lz_ != oceanSize)
				return true;
			if (l_ != shortestWaveLength)
				return true;
			if (A_ != amplitude)
				return true;
			if (V_ != windSpeed)
				return true;
			if (WHat_ != GetWindDirectionNormalized(windDirection))
				return true;
			if (alignement_ != alignement)
				return true;
			if (dampReflect_ != damp)
				return true;
			if (seed_ != seed)
				return true;
			return false;
		}

		MAXON_METHOD maxon::Result<void> Animate(maxon::Float currentTime, maxon::Int32 loopPeriod, maxon::Float timeScale, maxon::Float oceanDepth, maxon::Float chopAmount, maxon::Bool doDisp, maxon::Bool doChop, maxon::Bool doJacob, maxon::Bool doNormals)
		{
			iferr_scope_handler
			{
				ApplicationOutput("HOT4D DEBUG: OceanImplementation::Animate failed: @", err);
				return err;
			};

			Omega0_ = maxon::PI2 / (loopPeriod * timeScale);
			oceanDepth_ = oceanDepth;
			doDisp_ = doDisp;
			doNormals_ = doNormals;
			doChop_ = doChop;
			doJacob_ = doJacob;

			maxon::Int tileSize = maxon::Max<maxon::Int>(1, M_ / 8);

			auto prepareDisp = [&currentTime, &timeScale, this](maxon::Int i, maxon::Int j)
			{
				maxon::Float omega = Omega(km_(i, j));
				maxon::Complex<maxon::Float> complexOmega;
				maxon::Complex<maxon::Float> complexMinusOmega;
				complexOmega.SetExp(omega * currentTime * timeScale);
				complexMinusOmega.SetExp(-omega * currentTime * timeScale);

				htilda_(i, j) = h0_(i, j) * complexOmega + h0_minus_(i, j).GetConjugate() * complexMinusOmega;
				fftin_(i, j) = htilda_(i, j) * scaleNorm_;
			};
			maxon::ParallelImage::Process(M_, N_, tileSize, prepareDisp);

			const maxon::FFTRef KissFFT = maxon::FFTClasses::Kiss().Create() iferr_return;
			const maxon::FFT_SUPPORT options = KissFFT.GetSupportOptions();
			if (!(options & maxon::FFT_SUPPORT::TRANSFORM_2D))
				return maxon::UnexpectedError(MAXON_SOURCE_LOCATION);

			const maxon::Float sign[2] = { 1.0 , -1.0 };

			if (doDisp_)
			{
				KissFFT.Transform2D(fftin_, dispY_, fft_flags_) iferr_return;
				auto invertSignY = [&sign, this](maxon::Int32 l)
				{
					maxon::Int32 i = l / M_;
					maxon::Int32 j = maxon::Mod(l, M_);
					dispY_(i, j) *= sign[(i + j) & 1];
				};
				maxon::ParallelFor::Dynamic(0, M_ * N_, invertSignY);
			}

			if (doChop_)
			{
				auto prepareChopX = [this, &chopAmount](maxon::Int i, maxon::Int j)
				{
					fftin_(i, j) = chopAmount * -scaleNorm_ * maxon::Complex<maxon::Float>(0, -1) * htilda_(i, j) *
						(km_(i, j) == 0.0 ? maxon::Complex<maxon::Float>(0, 0) : kv_(i, j).x / km_(i, j));
				};
				maxon::ParallelImage::Process(M_, N_, tileSize, prepareChopX);
				KissFFT.Transform2D(fftin_, dispX_, fft_flags_) iferr_return;

				auto prepareChopZ = [this, &chopAmount](maxon::Int i, maxon::Int j)
				{
					fftin_(i, j) = chopAmount * -scaleNorm_ * maxon::Complex<maxon::Float>(0, -1) * htilda_(i, j) *
						(km_(i, j) == 0.0 ? maxon::Complex<maxon::Float>(0, 0) : kv_(i, j).y / km_(i, j));
				};
				maxon::ParallelImage::Process(M_, N_, tileSize, prepareChopZ);
				KissFFT.Transform2D(fftin_, dispZ_, fft_flags_) iferr_return;

				auto invertSignXZ = [&sign, this](maxon::Int32 l)
				{
					maxon::Int32 i = l / M_;
					maxon::Int32 j = maxon::Mod(l, M_);
					dispX_(i, j) *= sign[(i + j) & 1];
					dispZ_(i, j) *= sign[(i + j) & 1];
				};
				maxon::ParallelFor::Dynamic(0, M_ * N_, invertSignXZ);
			}

			if (doJacob_)
			{
				auto prepareJXX = [this, &chopAmount](maxon::Int i, maxon::Int j)
				{
					fftin_(i, j) = chopAmount * -scaleNorm_ * htilda_(i, j) *
						(km_(i, j) == 0.0 ? maxon::Complex<maxon::Float>(0, 0) : kv_(i, j).x * kv_(i, j).x / km_(i, j));
				};
				maxon::ParallelImage::Process(M_, N_, tileSize, prepareJXX);
				KissFFT.Transform2D(fftin_, jxx_, fft_flags_) iferr_return;

				auto prepareJZZ = [this, &chopAmount](maxon::Int i, maxon::Int j)
				{
					fftin_(i, j) = chopAmount * -scaleNorm_ * htilda_(i, j) *
						(km_(i, j) == 0.0 ? maxon::Complex<maxon::Float>(0, 0) : kv_(i, j).y * kv_(i, j).y / km_(i, j));
				};
				maxon::ParallelImage::Process(M_, N_, tileSize, prepareJZZ);
				KissFFT.Transform2D(fftin_, jzz_, fft_flags_) iferr_return;

				auto prepareJXZ = [this, &chopAmount](maxon::Int i, maxon::Int j)
				{
					fftin_(i, j) = chopAmount * -scaleNorm_ * htilda_(i, j) *
						(km_(i, j) == 0.0 ? maxon::Complex<maxon::Float>(0, 0) : kv_(i, j).x * kv_(i, j).y / km_(i, j));
				};
				maxon::ParallelImage::Process(M_, N_, tileSize, prepareJXZ);
				KissFFT.Transform2D(fftin_, jxz_, fft_flags_) iferr_return;

				auto invertSignJ = [&sign, this](maxon::Int32 l)
				{
					maxon::Int32 i = l / M_;
					maxon::Int32 j = maxon::Mod(l, M_);
					jxx_(i, j) *= sign[(i + j) & 1];
					jzz_(i, j) *= sign[(i + j) & 1];
					jxz_(i, j) *= sign[(i + j) & 1];
				};
				maxon::ParallelFor::Dynamic(0, M_ * N_, invertSignJ);

				auto calculateJminus = [this](maxon::Int32 l)
				{
					maxon::Int32 i = l / M_;
					maxon::Int32 j = maxon::Mod(l, M_);
					maxon::Float a = jxx_(i, j) + jzz_(i, j);
					maxon::Float b = maxon::Sqrt((jxx_(i, j) - jzz_(i, j)) * (jxx_(i, j) - jzz_(i, j)) + 4.0 * jxz_(i, j) * jxz_(i, j));
					jMinus_(i, j) = 0.5 * (a - b);
				};
				maxon::ParallelFor::Dynamic(0, M_ * N_, calculateJminus);
			}

			lastFrameCompute_ = currentTime;
			return maxon::OK;
		}

		MAXON_METHOD maxon::Result<void> EvaluatePoint(const INTERTYPE type, const maxon::Vector p, maxon::Vector& displacement, maxon::Vector& normal, maxon::Float& jMinus) const
		{
			if (Lx_ == 0.0 || Lz_ == 0.0)
				return maxon::IllegalArgumentError(MAXON_SOURCE_LOCATION);
			return self.EvaluateUV(type, maxon::Vector2d(p.x / Lx_, p.z / Lz_), displacement, normal, jMinus);
		}

		MAXON_METHOD maxon::Result<void> EvaluateUV(const INTERTYPE type, maxon::Vector2d uv, maxon::Vector& displacement, maxon::Vector& normal, maxon::Float& jMinus) const
		{
			iferr_scope;

			maxon::Int32 i0, i1, j0, j1;
			maxon::Int32 ia, i2, ja, j2;
			maxon::Float frac_x, frac_z;

			uv.x = maxon::FMod(uv.x, 1.0);
			uv.y = maxon::FMod(uv.y, 1.0);
			if (uv.x < 0)
				uv.x += 1.0;
			if (uv.y < 0)
				uv.y += 1.0;

			maxon::Float uu = uv.x * M_;
			maxon::Float vv = uv.y * N_;
			i0 = maxon::SafeConvert<maxon::Int32>(maxon::Floor(uu));
			j0 = maxon::SafeConvert<maxon::Int32>(maxon::Floor(vv));
			i1 = i0 + 1;
			j1 = j0 + 1;
			frac_x = uu - i0;
			frac_z = vv - j0;
			i0 = maxon::Mod(i0, M_);
			j0 = maxon::Mod(j0, N_);
			i1 = maxon::Mod(i1, M_);
			j1 = maxon::Mod(j1, N_);

			maxon::BaseArray<maxon::Int32> points;
			points.EnsureCapacity(8) iferr_ignore("append anyway");
			points.Append(i0) iferr_return;
			points.Append(i1) iferr_return;
			points.Append(j0) iferr_return;
			points.Append(j1) iferr_return;

			if (type == INTERTYPE::CATMULLROM)
			{
				ia = i0 - 1;
				i2 = i1 + 1;
				ia = ia < 0 ? ia + M_ : ia;
				i2 = i2 >= M_ ? i2 - M_ : i2;
				ja = j0 - 1;
				j2 = j1 + 1;
				ja = ja < 0 ? ja + N_ : ja;
				j2 = j2 >= N_ ? j2 - N_ : j2;
				points.Flush();
				points.Append(ia) iferr_return;
				points.Append(i0) iferr_return;
				points.Append(i1) iferr_return;
				points.Append(i2) iferr_return;
				points.Append(ja) iferr_return;
				points.Append(j0) iferr_return;
				points.Append(j1) iferr_return;
				points.Append(j2) iferr_return;
			}

			displacement = maxon::Vector(0.0);
			normal = maxon::Vector(0.0, 1.0, 0.0);
			jMinus = 0.0;

			if (doDisp_)
				displacement.y = Interpolation(type, dispY_, points, frac_x, frac_z);
			if (doNormals_)
			{
				normal[0] = Interpolation(type, normX_, points, frac_x, frac_z);
				normal[1] = Interpolation(type, normY_, points, frac_x, frac_z);
				normal[2] = Interpolation(type, normZ_, points, frac_x, frac_z);
			}
			if (doChop_)
			{
				displacement.x = Interpolation(type, dispX_, points, frac_x, frac_z);
				displacement.z = Interpolation(type, dispZ_, points, frac_x, frac_z);
			}
			if (doJacob_)
				jMinus = Interpolation(type, jMinus_, points, frac_x, frac_z);

			return maxon::OK;
		}

	private:
		maxon::Int32 LT_ = 0;
		maxon::Float Omega0_ = 0.0;
		maxon::Int32 M_ = 0;
		maxon::Int32 N_ = 0;
		maxon::Float Lx_ = 0.0;
		maxon::Float Lz_ = 0.0;
		maxon::Float l_ = 0.0;
		maxon::Float A_ = 0.0;
		maxon::Float V_ = 0.0;
		maxon::Vector2d WHat_;
		maxon::Float CA_ = 0.0;
		maxon::Float oceanDepth_ = 0.0;
		maxon::Int32 seed_ = 0;

		maxon::MatrixNxM<maxon::Vector2d> kv_;
		maxon::MatrixNxM<maxon::Float> km_;
		maxon::Float alignement_ = 0.0;
		maxon::Float dampReflect_ = 0.0;
		maxon::Float scaleNorm_ = 1.0;
		maxon::Bool doDisp_ = true;
		maxon::Bool doChop_ = false;
		maxon::Bool doNormals_ = false;
		maxon::Bool doJacob_ = false;

		maxon::MatrixNxM<maxon::Complex<maxon::Float>> h0_;
		maxon::MatrixNxM<maxon::Complex<maxon::Float>> h0_minus_;
		maxon::MatrixNxM<maxon::Complex<maxon::Float>> htilda_;
		maxon::MatrixNxM<maxon::Complex<maxon::Float>> fftin_;
		maxon::MatrixNxM<maxon::Float> dispX_;
		maxon::MatrixNxM<maxon::Float> dispY_;
		maxon::MatrixNxM<maxon::Float> dispZ_;
		maxon::MatrixNxM<maxon::Float> normX_;
		maxon::MatrixNxM<maxon::Float> normY_;
		maxon::MatrixNxM<maxon::Float> normZ_;
		maxon::MatrixNxM<maxon::Float> jxx_;
		maxon::MatrixNxM<maxon::Float> jzz_;
		maxon::MatrixNxM<maxon::Float> jxz_;
		maxon::MatrixNxM<maxon::Float> foam_;
		maxon::MatrixNxM<maxon::Float> jMinus_;
		maxon::Float lastFrameCompute_ = 0.0;

		const maxon::FFT_FLAGS fft_flags_ = maxon::FFT_FLAGS::CALC_INVERSE;

		maxon::Float Omega(maxon::Float k)
		{
			maxon::Float omegaK = maxon::Sqrt(gravity * k * maxon::Tanh(k * oceanDepth_));
			return maxon::Floor(omegaK / Omega0_) * Omega0_;
		}

		maxon::Float Phillips(maxon::Vector2d k)
		{
			maxon::Float k2 = k.GetSquaredLength();
			maxon::Vector2d knorm = k.GetNormalized();
			if (k2 == 0.0)
				return 0.0;
			maxon::Float L = maxon::Sqr(V_) / gravity;
			maxon::Float k4 = maxon::Sqr(k2);
			maxon::Float dot_k_wHat = knorm.x * WHat_.x + knorm.y * WHat_.y;
			if (dot_k_wHat < 0.0)
				dot_k_wHat *= maxon::Clamp01(1.0 - dampReflect_);
			return A_ * maxon::Exp(-1.0 / (k2 * maxon::Sqr(L)) - k2 * maxon::Sqr(l_)) / k4 * maxon::Pow(maxon::Abs(dot_k_wHat), alignement_);
		}

		maxon::Float Interpolation(const INTERTYPE type, const maxon::MatrixNxM<maxon::Float>& m, const maxon::BaseArray<maxon::Int32>& pointIndex, const maxon::Float f1, const maxon::Float f2) const
		{
			switch (type)
			{
				case INTERTYPE::LINEAR:
				{
					if (pointIndex.GetCount() < 4)
						return 1.0;
					maxon::Int32 i0 = pointIndex[0], i1 = pointIndex[1], j0 = pointIndex[2], j1 = pointIndex[3];
					return LinearInterpolation(LinearInterpolation(m(i0, j0), m(i1, j0), f1), LinearInterpolation(m(i0, j1), m(i1, j1), f1), f2);
				}
				case INTERTYPE::CATMULLROM:
				{
					if (pointIndex.GetCount() < 8)
						return 1.0;
					maxon::Int32 i0 = pointIndex[0], i1 = pointIndex[1], i2 = pointIndex[2], i3 = pointIndex[3];
					maxon::Int32 j0 = pointIndex[4], j1 = pointIndex[5], j2 = pointIndex[6], j3 = pointIndex[7];
					return CatmullRomInterpolation(
						CatmullRomInterpolation(m(i0, j0), m(i1, j0), m(i2, j0), m(i3, j0), f1),
						CatmullRomInterpolation(m(i0, j1), m(i1, j1), m(i2, j1), m(i3, j1), f1),
						CatmullRomInterpolation(m(i0, j2), m(i1, j2), m(i2, j2), m(i3, j2), f1),
						CatmullRomInterpolation(m(i0, j3), m(i1, j3), m(i2, j3), m(i3, j3), f1),
						f2);
				}
				default:
					return 0.0;
			}
		}

		maxon::Float CalculateNormalizedScale()
		{
			maxon::Float res = 1.0;
			iferr_scope_handler
			{
				DiagnosticOutput("Error: @", err);
				return res;
			};

			maxon::Complex<maxon::Float> complexOmega(1, 0);
			maxon::Complex<maxon::Float> complexMinusOmega(1, 0);

			auto prepareFFTIN = [this, &complexOmega, &complexMinusOmega](maxon::Int32 l)
			{
				maxon::Int32 i = l / M_;
				maxon::Int32 j = maxon::Mod(l, M_);
				fftin_(i, j) = h0_(i, j) * complexOmega + h0_minus_(i, j).GetConjugate() * complexMinusOmega;
			};
			maxon::ParallelFor::Dynamic(0, M_ * N_, prepareFFTIN);

			const maxon::FFTRef KissFFT = maxon::FFTClasses::Kiss().Create() iferr_return;
			const maxon::FFT_SUPPORT options = KissFFT.GetSupportOptions();
			if (!(options & maxon::FFT_SUPPORT::TRANSFORM_2D))
				return res;

			KissFFT.Transform2D(fftin_, dispY_, fft_flags_) iferr_return;

			maxon::Float maxHeight = std::numeric_limits<maxon::Float>::min();
			for (maxon::Int32 i = 0; i < dispY_.GetXCount(); i++)
				for (maxon::Int32 j = 0; j < dispY_.GetYCount(); j++)
					maxHeight = maxon::Max(maxHeight, maxon::Abs(dispY_(i, j)));

			if (maxHeight == 0.0)
				maxHeight = 0.00001;
			res = A_ / maxHeight;
			return res;
		}

		maxon::Vector2d GetWindDirectionNormalized(const maxon::Float windDirection) const
		{
			maxon::Vector2d windVector(maxon::Cos(windDirection), -maxon::Sin(windDirection));
			windVector.Normalize();
			return windVector;
		}
	};

	MAXON_COMPONENT_CLASS_REGISTER(OceanImplementation, Ocean);

	maxon::Result<OceanRef> CreateOceanDirect()
	{
		const auto& implClass = OceanImplementation::GetClass();
		ApplicationOutput("HOT4D DEBUG: OceanImplementation::GetClass id=@"_s, implClass.GetId());
		iferr (auto implOcean = implClass.Create())
		{
			ApplicationOutput("HOT4D DEBUG: CreateOceanDirect failed: @", err);
			return err;
		}

		return implOcean;
	}

	maxon::Result<void> RunOceanImplementationSelfTest()
	{
		iferr (auto implOcean = CreateOceanDirect())
		{
			ApplicationOutput("HOT4D DEBUG: RunOceanImplementationSelfTest failed: @", err);
			return err;
		}

		return maxon::OK;
	}

} // namespace OceanSimulation
