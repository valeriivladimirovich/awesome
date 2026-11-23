#include "World/LBCityGen.h"
#include "Core/LBGameData.h"

#define LOCTEXT_NAMESPACE "LastBlock"

namespace
{
	using G = ELBGround;
	using E = ELBEdge;
	using F = ELBFurniture;
	using P = ELBProp;
	constexpr int32 N = FLBCity::N;

	// Footprint half-size (tiles) and whether the prop blocks movement
	struct FPropDef { float HX, HY; bool bSolid; };
	const FPropDef& PropDef(P Kind)
	{
		static const FPropDef Defs[(int32)P::Count] = {
			{.2f,.2f,1}, {.15f,.15f,1}, {.7f,.22f,1}, {1,.3f,1}, {.2f,.2f,1}, {.08f,.08f,1},
			{.8f,.12f,1}, {.25f,.25f,1}, {.28f,.28f,1}, {.7f,.4f,1}, {.2f,.2f,1}, {.2f,.2f,0}, {.45f,.12f,0},
			{.3f,.3f,0}, {.8f,.3f,1}, {1,.15f,0}, {.7f,.5f,1}, {.3f,.2f,1}, {.1f,.1f,1}, {.1f,.1f,1},
			{1,1,1}, {.42f,.42f,1}, {.45f,.45f,0}, {.12f,.12f,0}, {.5f,.25f,1},
			{.1f,.1f,1}, {.08f,.08f,1}, {.5f,.22f,1}, {.25f,.25f,1}
		};
		return Defs[(int32)Kind];
	}

	FLinearColor Hex(const TCHAR* H) { return FLinearColor::FromSRGBColor(FColor::FromHex(H)); }

	struct FRoom { int32 X, Y, W, H; int32 Area() const { return W * H; } };

	struct FGen
	{
		FLBCity& C;
		FRandomStream R;
		TArray<uint8> Occ;
		TSet<int32> Clear;          // tiles kept free in front of doors
		TArray<int32> Roads;
		TArray<uint8> RoadI;

		FGen(FLBCity& InC, int32 Seed) : C(InC), R(Seed)
		{
			for (int32 K = 0; K < 9; ++K) Roads.Add(6 + 16 * K);
			RoadI.SetNumZeroed(N);
			for (int32 Rd : Roads) { RoadI[Rd] = 1; RoadI[Rd + 1] = 1; }
			Occ.SetNumZeroed(N * N);
		}

		static int32 T(int32 I, int32 J) { return J * N + I; }
		static bool Inb(int32 I, int32 J) { return FLBCity::InBounds(I, J); }
		float Rand(float A, float B) { return R.FRandRange(A, B); }
		int32 Ri(int32 A, int32 B) { return R.RandRange(A, B); }
		template <typename TT> const TT& Pick(const TArray<TT>& A) { return A[R.RandRange(0, A.Num() - 1)]; }
		bool Chance(float P) { return R.FRand() < P; }
		bool IsFree(int32 I, int32 J) const { return Inb(I, J) && !Occ[T(I, J)]; }
		bool AreaFree(float X0, float Y0, float X1, float Y1) const
		{
			for (int32 J = FMath::FloorToInt(Y0); J < FMath::CeilToInt(Y1); ++J)
				for (int32 I = FMath::FloorToInt(X0); I < FMath::CeilToInt(X1); ++I)
					if (!IsFree(I, J)) return false;
			return true;
		}
		void SetG(int32 I, int32 J, G V) { if (Inb(I, J)) C.Ground[T(I, J)] = V; }
		void SetH(int32 I, int32 J, E V, bool bForce = false)
		{
			if (I < 0 || I >= N || J < 0 || J > N) return;
			uint8& Slot = C.HEdge[J * N + I]; if (bForce || !Slot) Slot = (uint8)V;
		}
		void SetV(int32 I, int32 J, E V, bool bForce = false)
		{
			if (I < 0 || I > N || J < 0 || J >= N) return;
			uint8& Slot = C.VEdge[J * (N + 1) + I]; if (bForce || !Slot) Slot = (uint8)V;
		}
		void OccRect(float X0, float Y0, float X1, float Y1)
		{
			for (int32 J = FMath::FloorToInt(Y0); J < FMath::CeilToInt(Y1); ++J)
				for (int32 I = FMath::FloorToInt(X0); I < FMath::CeilToInt(X1); ++I)
					if (Inb(I, J)) Occ[T(I, J)] = 1;
		}

		// ---------- objects ----------
		void AddTree(float X, float Y, int32 Kind = -1, float Scale = 0.f)
		{
			const int32 I = FMath::FloorToInt(X), J = FMath::FloorToInt(Y);
			if (!Inb(I, J) || Occ[T(I, J)]) return;
			Occ[T(I, J)] = 1;
			FLBTreeSpot S; S.Pos = FVector2D(X, Y);
			S.Kind = Kind >= 0 ? Kind : (Chance(.25f) ? 1 : Chance(.3f) ? 2 : 0);
			S.Scale = Scale > 0.f ? Scale : (S.Kind == 2 ? Rand(.75f, .95f) : Rand(.85f, 1.2f));
			C.Trees.Add(S);
		}
		FLBCarSpot& AddCar(float X, float Y, bool bAlongY, ELBCarKind Kind = ELBCarKind::Sedan, bool bRandomBody = true)
		{
			static const TArray<FLinearColor> Paints = { Hex(TEXT("8e3d34")), Hex(TEXT("3b5b7a")), Hex(TEXT("c8b17d")), Hex(TEXT("4f5f4a")), Hex(TEXT("cfcfcb")), Hex(TEXT("6b4a6e")), Hex(TEXT("2f3238")) };
			const bool bBig = Kind == ELBCarKind::Truck || Kind == ELBCarKind::Ambulance || Kind == ELBCarKind::MilitaryTruck;
			if (Kind == ELBCarKind::Sedan && bRandomBody)
			{
				static const ELBCarKind Bodies[] = { ELBCarKind::Sedan, ELBCarKind::Sedan, ELBCarKind::Hatch, ELBCarKind::SUV, ELBCarKind::Pickup };
				Kind = Bodies[Ri(0, 4)];
			}
			FLBCarSpot& Car = C.Cars.AddDefaulted_GetRef();
			Car.Pos = FVector2D(X, Y); Car.bAlongY = bAlongY; Car.Kind = Kind; Car.Paint = Pick(Paints);
			const bool bCivil = Kind <= ELBCarKind::Pickup;
			Car.bWreck = bCivil && Chance(.3f);
			Car.bBurnt = bCivil && Chance(.15f);
			const float LX = bAlongY ? (bBig ? .62f : .5f) : (bBig ? 1.9f : 1.22f);
			const float LY = bAlongY ? (bBig ? 1.9f : 1.22f) : (bBig ? .62f : .5f);
			OccRect(X - LX, Y - LY, X + LX, Y + LY);
			return Car;
		}
		void AddLamp(float X, float Y)
		{
			FLBLampSpot L; L.Pos = FVector2D(X, Y); L.bOn = Chance(.72f); L.Phase = Rand(0, 10);
			C.Lamps.Add(L); Occ[T(FMath::FloorToInt(X), FMath::FloorToInt(Y))] = 1;
		}
		void AddProp(P Kind, float X, float Y, bool bRot = false)
		{
			const FPropDef& D = PropDef(Kind);
			FLBPropSpot S; S.Kind = Kind; S.Pos = FVector2D(X, Y); S.bRotated = bRot; S.bSolid = D.bSolid;
			S.Half = bRot ? FVector2D(D.HY, D.HX) : FVector2D(D.HX, D.HY);
			C.Props.Add(S);
			const int32 I = FMath::FloorToInt(X), J = FMath::FloorToInt(Y);
			if (Inb(I, J)) Occ[T(I, J)] = 1;
		}

		// ---------- buildings ----------
		struct FDoor { TCHAR Side; int32 At; };

		int32 Building(int32 X, int32 Y, int32 W, int32 H, ELBBuildingType Type, const TArray<FDoor>& Doors, float Win = .45f, bool bFrontS = false)
		{
			static const TArray<TArray<FLinearColor>> HouseWalls = { {Hex(TEXT("dccaa8"))}, {Hex(TEXT("a9bec2"))}, {Hex(TEXT("cf9670"))}, {Hex(TEXT("ddd6c8"))}, {Hex(TEXT("b9c49b"))}, {Hex(TEXT("c7b8d0"))} };
			static const TArray<FLinearColor> ComWalls = { Hex(TEXT("cfc6b8")), Hex(TEXT("b9bfc6")), Hex(TEXT("d8cdb5")), Hex(TEXT("c4b29c")) };
			static const TArray<FLinearColor> HouseRoofs = { Hex(TEXT("6a3f36")), Hex(TEXT("3e5563")), Hex(TEXT("77503b")), Hex(TEXT("4b4c57")), Hex(TEXT("5d5d3c")), Hex(TEXT("7d4a3a")) };
			static const TArray<FLinearColor> ComRoofs = { Hex(TEXT("5a5c60")), Hex(TEXT("4e5156")), Hex(TEXT("62625c")) };

			const bool bCom = !(Type == ELBBuildingType::House || Type == ELBBuildingType::Shed || Type == ELBBuildingType::Tent);
			const G Floor = (Type == ELBBuildingType::House || Type == ELBBuildingType::Cafe) ? G::Wood
				: (Type == ELBBuildingType::Shed || Type == ELBBuildingType::Hardware || Type == ELBBuildingType::Warehouse) ? G::Concrete
				: Type == ELBBuildingType::Tent ? G::Dirt : G::Tile;

			FLBBuilding B;
			B.Rect = FIntRect(X, Y, X + W, Y + H);
			B.Type = Type;
			B.bFlatRoof = bCom;
			B.WallColor = bCom ? Pick(ComWalls) : Pick(HouseWalls)[0];
			B.RoofColor = bCom ? Pick(ComRoofs) : Pick(HouseRoofs);
			B.bLit = Chance(.35f);
			switch (Type)
			{
			case ELBBuildingType::Shop: B.SignName = LOCTEXT("SShop", "Продукты"); B.SignColor = Hex(TEXT("c8473a")); break;
			case ELBBuildingType::Pharmacy: B.SignName = LOCTEXT("SPharm", "Аптека"); B.SignColor = Hex(TEXT("3c9a5f")); break;
			case ELBBuildingType::Hardware: B.SignName = LOCTEXT("SHard", "Хозтовары"); B.SignColor = Hex(TEXT("d08a2c")); break;
			case ELBBuildingType::Cafe: B.SignName = LOCTEXT("SCafe", "Кафе"); B.SignColor = Hex(TEXT("8a5a9e")); break;
			case ELBBuildingType::GasShop: B.SignName = LOCTEXT("SGas", "АЗС"); B.SignColor = Hex(TEXT("2f6fb0")); break;
			case ELBBuildingType::Warehouse: B.SignName = LOCTEXT("SWare", "Склад"); B.SignColor = Hex(TEXT("6b6f78")); break;
			case ELBBuildingType::Police: B.SignName = LOCTEXT("SPolice", "Полиция"); B.SignColor = Hex(TEXT("2b4a8a")); break;
			case ELBBuildingType::Clinic: B.SignName = LOCTEXT("SClinic", "Больница"); B.SignColor = Hex(TEXT("c23b3b")); break;
			case ELBBuildingType::Tent: B.WallColor = Hex(TEXT("6b6e4a")); B.RoofColor = Hex(TEXT("565a38")); break;
			default: break;
			}
			const int32 Id = C.Buildings.Add(B);

			for (int32 J = Y; J < Y + H; ++J) for (int32 I = X; I < X + W; ++I) { C.Ground[T(I, J)] = Floor; C.Bld[T(I, J)] = Id; Occ[T(I, J)] = 1; }
			auto WinOrWall = [&]() { return Chance(Win) ? E::Window : E::Solid; };
			for (int32 I = X; I < X + W; ++I) { SetH(I, Y, WinOrWall(), true); SetH(I, Y + H, WinOrWall(), true); }
			for (int32 J = Y; J < Y + H; ++J) { SetV(X, J, WinOrWall(), true); SetV(X + W, J, WinOrWall(), true); }
			if (bFrontS) for (int32 I = X; I < X + W; ++I) SetH(I, Y + H, E::Window, true);
			for (const FDoor& D : Doors)
			{
				int32 OI = 0, OJ = 0;
				switch (D.Side)
				{
				case 'S': SetH(D.At, Y + H, E::Door, true); OI = D.At; OJ = Y + H; break;
				case 'N': SetH(D.At, Y, E::Door, true); OI = D.At; OJ = Y - 1; break;
				case 'E': SetV(X + W, D.At, E::Door, true); OI = X + W; OJ = D.At; break;
				default:  SetV(X, D.At, E::Door, true); OI = X - 1; OJ = D.At; break;
				}
				if (Inb(OI, OJ)) { if (C.Ground[T(OI, OJ)] == G::Grass) C.Ground[T(OI, OJ)] = G::Dirt; Occ[T(OI, OJ)] = 1; }
			}
			return Id;
		}

		TArray<FRoom> SplitRooms(int32 Bi, int32 Depth)
		{
			const FIntRect& Rc = C.Buildings[Bi].Rect;
			TArray<FRoom> Rooms = { { Rc.Min.X, Rc.Min.Y, Rc.Width(), Rc.Height() } };
			for (int32 D = 0; D < Depth; ++D)
			{
				Rooms.Sort([](const FRoom& A, const FRoom& B) { return A.Area() > B.Area(); });
				const FRoom Rm = Rooms[0];
				if (Rm.W >= 6 && Rm.W >= Rm.H)
				{
					const int32 K = Rm.X + Ri(3, Rm.W - 3), DJ = Rm.Y + Ri(0, Rm.H - 1);
					for (int32 J = Rm.Y; J < Rm.Y + Rm.H; ++J) SetV(K, J, J == DJ ? E::Door : E::Solid, true);
					Rooms[0] = { Rm.X, Rm.Y, K - Rm.X, Rm.H };
					Rooms.Insert({ K, Rm.Y, Rm.X + Rm.W - K, Rm.H }, 1);
				}
				else if (Rm.H >= 6)
				{
					const int32 K = Rm.Y + Ri(3, Rm.H - 3), DI = Rm.X + Ri(0, Rm.W - 1);
					for (int32 I = Rm.X; I < Rm.X + Rm.W; ++I) SetH(I, K, I == DI ? E::Door : E::Solid, true);
					Rooms[0] = { Rm.X, Rm.Y, Rm.W, K - Rm.Y };
					Rooms.Insert({ Rm.X, K, Rm.W, Rm.Y + Rm.H - K }, 1);
				}
				else break;
			}
			return Rooms;
		}

		/** Keeps the tiles on both sides of every door free of furniture */
		void PrepClear(int32 Bi)
		{
			const FIntRect& B = C.Buildings[Bi].Rect;
			for (int32 J = B.Min.Y; J <= B.Max.Y; ++J) for (int32 I = B.Min.X; I < B.Max.X; ++I)
				if (C.HAt(I, J) == E::Door) { Clear.Add(T(I, J)); Clear.Add(T(I, J - 1)); }
			for (int32 J = B.Min.Y; J < B.Max.Y; ++J) for (int32 I = B.Min.X; I <= B.Max.X; ++I)
				if (C.VAt(I, J) == E::Door) { Clear.Add(T(I, J)); Clear.Add(T(I - 1, J)); }
		}

		bool CanPlace(int32 Bi, float X0, float Y0, float X1, float Y1) const
		{
			const FIntRect& B = C.Buildings[Bi].Rect;
			if (X0 < B.Min.X || Y0 < B.Min.Y || X1 > B.Max.X || Y1 > B.Max.Y) return false;
			for (const FLBFurniturePiece& Fp : C.Furniture)
			{
				if (Fp.Building != Bi) continue;
				if (X0 < Fp.Rect.Max.X + .22f && X1 > Fp.Rect.Min.X - .22f && Y0 < Fp.Rect.Max.Y + .22f && Y1 > Fp.Rect.Min.Y - .22f) return false;
			}
			for (int32 J = FMath::FloorToInt(Y0); J < FMath::CeilToInt(Y1 - 1e-6f); ++J)
				for (int32 I = FMath::FloorToInt(X0); I < FMath::CeilToInt(X1 - 1e-6f); ++I)
					if (Clear.Contains(T(I, J))) return false;
			return true;
		}

		void AddFurn(int32 Bi, F Kind, float X0, float Y0, float X1, float Y1, ELBSide Side, FName Loot = NAME_None)
		{
			const FLBFurnitureDef Def = ULBGameData::GetFurniture(Kind);
			FLBFurniturePiece Fp;
			Fp.Kind = Kind; Fp.Rect = FBox2D(FVector2D(X0, Y0), FVector2D(X1, Y1)); Fp.Side = Side; Fp.Building = Bi;
			Fp.LootTable = Loot.IsNone() ? Def.LootTable : Loot;
			const int32 Idx = C.Furniture.Add(Fp);
			if (!Def.ContainerName.IsEmpty())
			{
				const bool bRich = Kind == F::Shelf || Kind == F::MilitaryCrate;
				FLBContainer& Ct = C.Containers.AddDefaulted_GetRef();
				Ct.Furniture = Idx; Ct.Name = Def.ContainerName; Ct.LootTable = Fp.LootTable;
				Ct.Loot = ULBGameData::RollLoot(Fp.LootTable, bRich ? 3 : 2, R);
			}
		}

		bool TryPlace(int32 Bi, const FRoom& Room, F Kind, int32 ForceSide = -1, FName Loot = NAME_None)
		{
			const FLBFurnitureDef Def = ULBGameData::GetFurniture(Kind);
			static const ELBSide RandomSides[] = { ELBSide::N, ELBSide::N, ELBSide::W, ELBSide::W, ELBSide::S, ELBSide::E };
			for (int32 Try = 0; Try < 40; ++Try)
			{
				const ELBSide Side = ForceSide >= 0 ? (ELBSide)ForceSide : (Def.bCentered ? ELBSide::C : RandomSides[Ri(0, 5)]);
				float W = Def.Size.X, H = Def.Size.Y;
				if (Side == ELBSide::W || Side == ELBSide::E) Swap(W, H);
				if (Side == ELBSide::C && Chance(.5f)) Swap(W, H);
				const float M = .08f, FW = Room.W - W - 2 * M, FH = Room.H - H - 2 * M;
				if (FW < 0 || FH < 0) continue;
				float X0, Y0;
				switch (Side)
				{
				case ELBSide::N: Y0 = Room.Y + M; X0 = Room.X + M + R.FRand() * FW; break;
				case ELBSide::S: Y0 = Room.Y + Room.H - H - M; X0 = Room.X + M + R.FRand() * FW; break;
				case ELBSide::W: X0 = Room.X + M; Y0 = Room.Y + M + R.FRand() * FH; break;
				case ELBSide::E: X0 = Room.X + Room.W - W - M; Y0 = Room.Y + M + R.FRand() * FH; break;
				default: X0 = Room.X + M + FW * Rand(.25f, .75f); Y0 = Room.Y + M + FH * Rand(.25f, .75f); break;
				}
				if (CanPlace(Bi, X0, Y0, X0 + W, Y0 + H)) { AddFurn(Bi, Kind, X0, Y0, X0 + W, Y0 + H, Side, Loot); return true; }
			}
			return false;
		}

		void Rug(const FRoom& Rm)
		{
			if (Rm.W >= 3 && Rm.H >= 3) C.Rugs.Add(FBox2D(FVector2D(Rm.X + .9f, Rm.Y + .9f), FVector2D(Rm.X + Rm.W - .9f, Rm.Y + Rm.H - .9f)));
		}

		void FurnishHouse(int32 Bi)
		{
			const FIntRect& B = C.Buildings[Bi].Rect;
			TArray<FRoom> Rooms = SplitRooms(Bi, B.Area() >= 24 ? 2 : 1); PrepClear(Bi);
			Rooms.Sort([](const FRoom& A, const FRoom& Bb) { return A.Area() > Bb.Area(); });
			const TArray<FName> Sets = Rooms.Num() == 1 ? TArray<FName>{ "studio" } : Rooms.Num() == 2 ? TArray<FName>{ "livkit", "bedroom" } : TArray<FName>{ "living", "kitchen", "bedroom", "bath" };
			for (int32 K = 0; K < Rooms.Num(); ++K)
			{
				const FName Name = Sets[FMath::Min(K, Sets.Num() - 1)];
				if (Name.ToString().StartsWith(TEXT("liv"))) Rug(Rooms[K]);
				for (F Kind : ULBGameData::RoomSet(Name)) TryPlace(Bi, Rooms[K], Kind);
			}
		}

		void ShelfRows(int32 Bi, FName Loot, float Step = 1.9f)
		{
			const FIntRect& B = C.Buildings[Bi].Rect;
			for (float YY = B.Min.Y + 1.2f; YY + .36f <= B.Max.Y - .9f; YY += 1.15f)
				for (float XX = B.Min.X + .7f; XX + 1.3f <= B.Max.X - .5f; XX += Step)
					if (CanPlace(Bi, XX, YY, XX + 1.3f, YY + .36f)) AddFurn(Bi, F::Shelf, XX, YY, XX + 1.3f, YY + .36f, ELBSide::C, Loot);
		}

		void FurnishCom(int32 Bi)
		{
			const FLBBuilding& B = C.Buildings[Bi];
			const FRoom Room = { B.Rect.Min.X, B.Rect.Min.Y, B.Rect.Width(), B.Rect.Height() };
			const int32 N_ = (int32)ELBSide::N, W_ = (int32)ELBSide::W, S_ = (int32)ELBSide::S, E_ = (int32)ELBSide::E;
			if (B.Type == ELBBuildingType::Police || B.Type == ELBBuildingType::Clinic)
			{
				const bool bPolice = B.Type == ELBBuildingType::Police;
				TArray<FRoom> Rooms = SplitRooms(Bi, 2); PrepClear(Bi);
				Rooms.Sort([](const FRoom& A, const FRoom& Bb) { return A.Area() > Bb.Area(); });
				for (int32 K = 0; K < Rooms.Num(); ++K)
				{
					const FRoom& Rm = Rooms[K];
					if (bPolice)
					{
						if (K == 0) { TryPlace(Bi, Rm, F::Locker, N_); TryPlace(Bi, Rm, F::Locker, N_); TryPlace(Bi, Rm, F::Desk, W_); TryPlace(Bi, Rm, F::Desk); TryPlace(Bi, Rm, F::Plant); }
						else { TryPlace(Bi, Rm, F::Desk); TryPlace(Bi, Rm, F::Bookshelf); TryPlace(Bi, Rm, F::Crate); TryPlace(Bi, Rm, F::Locker); }
					}
					else if (K < 2) { TryPlace(Bi, Rm, F::HospitalBed, N_); TryPlace(Bi, Rm, F::HospitalBed, N_); TryPlace(Bi, Rm, F::Cabinet, W_, "pharm"); TryPlace(Bi, Rm, F::Plant); }
					else { TryPlace(Bi, Rm, F::Desk); TryPlace(Bi, Rm, F::Cabinet, -1, "pharm"); TryPlace(Bi, Rm, F::Cabinet, -1, "pharm"); }
				}
				return;
			}
			PrepClear(Bi);
			switch (B.Type)
			{
			case ELBBuildingType::Shop: case ELBBuildingType::GasShop:
				for (int32 K = 0; K < 3; ++K) TryPlace(Bi, Room, F::Fridge, N_);
				ShelfRows(Bi, "shop"); TryPlace(Bi, Room, F::Checkout, S_, "counter"); break;
			case ELBBuildingType::Hardware:
				TryPlace(Bi, Room, F::Workbench, N_); ShelfRows(Bi, "hard"); TryPlace(Bi, Room, F::Crate, N_); TryPlace(Bi, Room, F::Crate, E_); break;
			case ELBBuildingType::Pharmacy:
				TryPlace(Bi, Room, F::Cabinet, N_, "pharm"); TryPlace(Bi, Room, F::Cabinet, N_, "pharm"); TryPlace(Bi, Room, F::Cabinet, W_, "pharm");
				ShelfRows(Bi, "pharm", 2.1f); TryPlace(Bi, Room, F::Checkout, S_, "pharm"); break;
			case ELBBuildingType::Cafe:
				TryPlace(Bi, Room, F::Counter, N_, "cafe"); TryPlace(Bi, Room, F::Counter, N_, "cafe"); TryPlace(Bi, Room, F::Fridge, N_);
				for (int32 K = 0; K < 5; ++K) TryPlace(Bi, Room, F::CoffeeTable);
				TryPlace(Bi, Room, F::Plant); break;
			case ELBBuildingType::Warehouse:
				ShelfRows(Bi, "crate", 2.2f);
				for (int32 K = 0; K < 8; ++K) TryPlace(Bi, Room, F::Crate, Ri(0, 3));
				break;
			case ELBBuildingType::Tent:
				TryPlace(Bi, Room, F::MilitaryCrate, N_); TryPlace(Bi, Room, F::MilitaryCrate, E_); TryPlace(Bi, Room, F::Bed, W_); break;
			case ELBBuildingType::Shed:
				TryPlace(Bi, Room, F::Workbench, N_); TryPlace(Bi, Room, F::Crate); break;
			default: break;
			}
		}

		void YardDeco(int32 LX, int32 LY, int32 Count)
		{
			static const TArray<P> Junk = { P::Bbq, P::Chair, P::Chair, P::TrashBags, P::Bike, P::Barrel, P::Tires };
			for (int32 K = 0; K < Count; ++K)
			{
				const int32 I = LX + Ri(0, 5), J = LY + Ri(0, 5);
				if (!IsFree(I, J) || C.Ground[T(I, J)] != G::Grass) continue;
				if (Chance(.45f)) AddTree(I + .5f + Rand(-.15f, .15f), J + .5f + Rand(-.15f, .15f), Chance(.35f) ? 2 : -1);
				else AddProp(Pick(Junk), I + .5f + Rand(-.2f, .2f), J + .5f + Rand(-.2f, .2f));
			}
		}

		// ---------- block generators (each block is 12x12 tiles) ----------
		void GenRes(int32 BX, int32 BY)
		{
			const int32 Lots[4][2] = { {BX, BY}, {BX + 6, BY}, {BX, BY + 6}, {BX + 6, BY + 6} };
			for (const auto& L : Lots)
			{
				const int32 LX = L[0], LY = L[1];
				TArray<TCHAR> Sides;
				if (LY == BY) Sides.Add('N');
				if (LY == BY + 6) Sides.Add('S');
				if (LX == BX) Sides.Add('W');
				if (LX == BX + 6) Sides.Add('E');
				if (Chance(.8f))
				{
					const TCHAR Side = Pick(Sides); const int32 W = Ri(4, 5), H = Ri(4, 5);
					int32 X, Y;
					if (Side == 'S') { Y = LY + 5 - H; X = LX + Ri(0, 6 - W); }
					else if (Side == 'N') { Y = LY + 1; X = LX + Ri(0, 6 - W); }
					else if (Side == 'E') { X = LX + 5 - W; Y = LY + Ri(0, 6 - H); }
					else { X = LX + 1; Y = LY + Ri(0, 6 - H); }
					const int32 At = (Side == 'S' || Side == 'N') ? X + Ri(1, W - 2) : Y + Ri(1, H - 2);
					const int32 Bi = Building(X, Y, W, H, ELBBuildingType::House, { { Side, At } });
					FurnishHouse(Bi);
					if (Chance(.5f))
					{
						int32 MI, MJ;
						if (Side == 'S') { MI = At + 1; MJ = LY + 5; } else if (Side == 'N') { MI = At + 1; MJ = LY; }
						else if (Side == 'E') { MI = LX + 5; MJ = At + 1; } else { MI = LX; MJ = At + 1; }
						if (IsFree(MI, MJ)) AddProp(P::Mailbox, MI + .5f, MJ + .5f);
					}
					YardDeco(LX, LY, Ri(2, 4));
				}
				else
				{
					const int32 SX = LX + Ri(0, 3), SY = LY + Ri(0, 3);
					const TCHAR Side = Chance(.5f) ? 'S' : 'E';
					const int32 Bi = Building(SX, SY, 3, 3, ELBBuildingType::Shed, { { Side, Side == 'S' ? SX + 1 : SY + 1 } }, .15f);
					FurnishCom(Bi);
					for (int32 K = 0; K < 3; ++K)
					{
						const int32 GX = LX + Ri(0, 4), GY = LY + Ri(0, 5);
						if (IsFree(GX, GY) && IsFree(GX + 1, GY) && C.Ground[T(GX, GY)] == G::Grass && C.Ground[T(GX + 1, GY)] == G::Grass)
						{
							C.Rugs.Add(FBox2D(FVector2D(GX + .1f, GY + .2f), FVector2D(GX + 1.9f, GY + .8f)));
							Occ[T(GX, GY)] = Occ[T(GX + 1, GY)] = 1;
						}
					}
					YardDeco(LX, LY, 3);
				}
				// back fences between lots, with a gap to squeeze through
				if (!Sides.Contains('N')) { const int32 Gap = LX + Ri(1, 4); for (int32 I = LX; I < LX + 6; ++I) if (I != Gap) SetH(I, LY, E::Fence); }
				if (!Sides.Contains('W')) { const int32 Gap = LY + Ri(1, 4); for (int32 J = LY; J < LY + 6; ++J) if (J != Gap) SetV(LX, J, E::Fence); }
			}
		}

		void GenShop(int32 BX, int32 BY)
		{
			const TArray<ELBBuildingType> Big = { ELBBuildingType::Shop, ELBBuildingType::Hardware, ELBBuildingType::Shop };
			const TArray<ELBBuildingType> Small = { ELBBuildingType::Pharmacy, ELBBuildingType::Cafe, ELBBuildingType::Pharmacy };
			const int32 B1 = Building(BX + 1, BY + 1, 10, 5, Pick(Big), { {'S', BX + 3}, {'S', BX + 8} }, .45f, true);
			for (int32 I = BX; I < BX + 12; ++I) { SetG(I, BY + 6, G::Concrete); Occ[T(I, BY + 6)] = 1; }
			const int32 B2 = Building(BX + 1, BY + 7, 6, 5, Pick(Small), { {'S', BX + 3} }, .45f, true);
			for (int32 J = BY + 7; J < BY + 12; ++J) { SetG(BX, J, G::Concrete); SetG(BX + 7, J, G::Concrete); Occ[T(BX + 7, J)] = 1; }
			for (int32 I = BX + 8; I < BX + 12; ++I) for (int32 J = BY + 7; J < BY + 12; ++J) SetG(I, J, G::Lot);
			for (float CY : { BY + 7.6f, BY + 9.5f, BY + 11.4f }) if (Chance(.6f)) AddCar(BX + 10.1f, CY, false);
			FurnishCom(B1); FurnishCom(B2);
			const TArray<P> Front = { P::Planter, P::Bench, P::Bin };
			for (int32 I = BX + 1; I < BX + 11; I += 3) if (Chance(.6f)) AddProp(Pick(Front), I + .5f, BY + .5f);
			AddProp(P::Dumpster, BX + 8.2f, BY + .55f);
		}

		void GenGas(int32 BX, int32 BY)
		{
			for (int32 J = BY; J < BY + 12; ++J) for (int32 I = BX; I < BX + 12; ++I) SetG(I, J, G::Concrete);
			FurnishCom(Building(BX + 6, BY + 1, 6, 4, ELBBuildingType::GasShop, { {'S', BX + 8} }, .45f, true));
			for (const FVector2D& Pp : { FVector2D(BX + 1.4f, BY + 6.4f), FVector2D(BX + 6.6f, BY + 6.4f), FVector2D(BX + 1.4f, BY + 9.6f), FVector2D(BX + 6.6f, BY + 9.6f) }) AddProp(P::Post, Pp.X, Pp.Y);
			for (float PX : { BX + 2.8f, BX + 5.2f }) AddProp(P::Pump, PX, BY + 8);
			if (Chance(.7f)) AddCar(BX + 4, BY + 9, false);
			AddProp(P::PriceSign, BX + .5f, BY + 11.4f);
			AddProp(P::Tires, BX + 11.3f, BY + 6.6f); AddProp(P::Barrel, BX + 11.4f, BY + 7.5f); AddProp(P::Barrel, BX + 10.7f, BY + 7.4f);
			AddProp(P::Dumpster, BX + 10.6f, BY + 10.9f); AddProp(P::Bin, BX + 6.5f, BY + 5.6f); AddProp(P::Cone, BX + 8.5f, BY + 10.5f);
		}

		void GenPark(int32 BX, int32 BY)
		{
			for (int32 K = 0; K < 12; ++K)
			{
				for (const FIntPoint& Q : { FIntPoint(BX + 5, BY + K), FIntPoint(BX + 6, BY + K), FIntPoint(BX + K, BY + 5), FIntPoint(BX + K, BY + 6) })
				{ SetG(Q.X, Q.Y, G::Dirt); Occ[T(Q.X, Q.Y)] = 1; }
			}
			AddProp(P::Fountain, BX + 6, BY + 6);
			const FIntPoint Quads[4] = { {BX, BY}, {BX + 7, BY}, {BX, BY + 7}, {BX + 7, BY + 7} };
			const FIntPoint Q = Quads[Ri(0, 3)];
			for (int32 J = Q.Y + 1; J < Q.Y + 4; ++J) for (int32 I = Q.X; I < Q.X + 4; ++I) { SetG(I, J, G::Sand); Occ[T(I, J)] = 1; }
			AddProp(P::Slide, Q.X + 1.2f, Q.Y + 2); AddProp(P::Swings, Q.X + 3, Q.Y + 3);
			AddProp(P::SwingPost, Q.X + 2, Q.Y + 3); AddProp(P::SwingPost, Q.X + 4, Q.Y + 3);
			AddProp(P::Bench, BX + 3, BY + 4.6f); AddProp(P::Bench, BX + 9, BY + 4.6f); AddProp(P::Bench, BX + 4.6f, BY + 9, true); AddProp(P::Bench, BX + 7.4f, BY + 2.5f, true);
			AddProp(P::Picnic, BX + 9.5f, BY + 9.5f); AddProp(P::Bin, BX + 4.6f, BY + 7.4f); AddProp(P::Bin, BX + 7.4f, BY + 4.6f);
			for (int32 K = 0; K < 22; ++K)
			{
				const int32 I = BX + Ri(0, 11), J = BY + Ri(0, 11);
				if (IsFree(I, J) && C.Ground[T(I, J)] == G::Grass) AddTree(I + .5f + Rand(-.2f, .2f), J + .5f + Rand(-.2f, .2f), Chance(.25f) ? 2 : Chance(.3f) ? 1 : 0);
			}
		}

		void GenWare(int32 BX, int32 BY)
		{
			FurnishCom(Building(BX + 1, BY + 1, 10, 7, ELBBuildingType::Warehouse, { {'S', BX + 4}, {'S', BX + 5}, {'E', BY + 3} }, .15f));
			for (int32 J = BY + 8; J < BY + 12; ++J) for (int32 I = BX; I < BX + 12; ++I) SetG(I, J, G::Concrete);
			AddCar(BX + 8, BY + 10.5f, false, ELBCarKind::Truck);
			const TArray<P> Yard = { P::CrateOut, P::CrateOut, P::Pallet, P::Barrel };
			for (int32 K = 0; K < 6; ++K) { const int32 I = BX + Ri(0, 5), J = BY + Ri(8, 11); if (IsFree(I, J)) AddProp(Pick(Yard), I + .5f, J + .5f); }
			AddProp(P::Dumpster, BX + 11.2f, BY + 9);
		}

		void GenPolice(int32 BX, int32 BY)
		{
			FurnishCom(Building(BX + 1, BY + 1, 10, 6, ELBBuildingType::Police, { {'S', BX + 5} }, .5f));
			for (int32 J = BY + 8; J < BY + 12; ++J) for (int32 I = BX; I < BX + 12; ++I) SetG(I, J, G::Lot);
			AddCar(BX + 2.5f, BY + 10, true, ELBCarKind::Police); AddCar(BX + 4.5f, BY + 10, true, ELBCarKind::Police);
			if (Chance(.6f)) AddCar(BX + 8.5f, BY + 10, true);
			AddProp(P::Barrier, BX + 6, BY + 11.3f); AddProp(P::Cone, BX + 7.5f, BY + 11); AddProp(P::Cone, BX + 9, BY + 11.2f);
		}

		void GenClinic(int32 BX, int32 BY)
		{
			FurnishCom(Building(BX + 1, BY + 1, 10, 7, ELBBuildingType::Clinic, { {'S', BX + 5}, {'E', BY + 3} }, .5f));
			for (int32 J = BY + 9; J < BY + 12; ++J) for (int32 I = BX; I < BX + 12; ++I) SetG(I, J, G::Concrete);
			AddCar(BX + 8, BY + 10, false, ELBCarKind::Ambulance);
			AddProp(P::Bench, BX + 3, BY + 9.5f); AddProp(P::Bin, BX + 1, BY + 9.5f); AddProp(P::Planter, BX + 5.5f, BY + 11.3f);
		}

		void GenCamp(int32 BX, int32 BY)
		{
			for (int32 J = BY; J < BY + 12; ++J) for (int32 I = BX; I < BX + 12; ++I) SetG(I, J, Chance(.3f) ? G::Sand : G::Dirt);
			for (int32 K = 0; K < 12; ++K) if (K != 5 && K != 6)
			{
				AddProp(P::Sandbag, BX + K + .5f, BY + .3f); AddProp(P::Sandbag, BX + K + .5f, BY + 11.7f);
				AddProp(P::Sandbag, BX + .3f, BY + K + .5f, true); AddProp(P::Sandbag, BX + 11.7f, BY + K + .5f, true);
			}
			FurnishCom(Building(BX + 2, BY + 2, 4, 3, ELBBuildingType::Tent, { {'S', BX + 3} }, 0.f));
			FurnishCom(Building(BX + 7, BY + 2, 4, 3, ELBBuildingType::Tent, { {'S', BX + 9} }, 0.f));
			AddCar(BX + 4, BY + 8.5f, false, ELBCarKind::MilitaryTruck);
			AddProp(P::Barrier, BX + 6, BY + 10.6f); AddProp(P::FireBarrel, BX + 6, BY + 6.5f); AddProp(P::Barrel, BX + 9.5f, BY + 7.5f);
			AddProp(P::Barrel, BX + 10.2f, BY + 7.8f); AddProp(P::CrateOut, BX + 9.6f, BY + 9.6f); AddProp(P::Cone, BX + 5, BY + 11);
		}

		void GenJunk(int32 BX, int32 BY)
		{
			for (int32 J = BY; J < BY + 12; ++J) for (int32 I = BX; I < BX + 12; ++I) SetG(I, J, Chance(.7f) ? G::Dirt : G::Grass);
			for (int32 K = 0; K < 12; ++K) if (K < 5 || K > 6) { SetH(BX + K, BY, E::Fence); SetH(BX + K, BY + 12, E::Fence); SetV(BX, BY + K, E::Fence); SetV(BX + 12, BY + K, E::Fence); }
			if (Chance(.8f)) FurnishCom(Building(BX + 8, BY + 8, 3, 3, ELBBuildingType::Shed, { {'W', BY + 9} }, .1f));
			for (int32 K = 0; K < 14; ++K)
			{
				const bool bAY = Chance(.5f); const int32 I = BX + Ri(1, 10), J = BY + Ri(1, 10); const float CX = I + .5f, CY = J + .5f;
				const bool bOk = bAY ? (AreaFree(CX - .5f, CY - 1.25f, CX + .5f, CY + 1.25f) && CY + 1.25f < BY + 12) : (AreaFree(CX - 1.25f, CY - .5f, CX + 1.25f, CY + .5f) && CX + 1.25f < BX + 12);
				if (bOk) { FLBCarSpot& Car = AddCar(CX, CY, bAY); Car.bWreck = true; Car.bBurnt = Chance(.5f); }
			}
			const TArray<P> Junk = { P::Tires, P::Tires, P::Barrel, P::TrashBags, P::CrateOut, P::Pallet };
			for (int32 K = 0; K < 16; ++K) { const int32 I = BX + Ri(0, 11), J = BY + Ri(0, 11); if (IsFree(I, J)) AddProp(Pick(Junk), I + .5f, J + .5f); }
			for (int32 K = 0; K < 2; ++K) { const int32 I = BX + Ri(1, 10), J = BY + Ri(1, 10); if (IsFree(I, J)) AddProp(P::FireBarrel, I + .5f, J + .5f); }
		}

		void Run()
		{
			C.Ground.Init(G::Grass, N * N);
			C.Bld.Init(-1, N * N);
			C.HEdge.SetNumZeroed(N * (N + 1));
			C.VEdge.SetNumZeroed((N + 1) * N);
			const int32 FEdge = Roads.Last() + 2;

			// terrain: roads, sidewalks along them, forest around the town
			for (int32 J = 0; J < N; ++J) for (int32 I = 0; I < N; ++I) if (RoadI[I] || RoadI[J]) C.Ground[T(I, J)] = G::Road;
			for (int32 J = 0; J < N; ++J) for (int32 I = 0; I < N; ++I)
			{
				if (C.Ground[T(I, J)] != G::Grass) continue;
				bool bNearRoad = false;
				for (int32 DJ = -1; DJ <= 1; ++DJ) for (int32 DI = -1; DI <= 1; ++DI)
					if ((DI || DJ) && Inb(I + DI, J + DJ) && C.Ground[T(I + DI, J + DJ)] == G::Road) bNearRoad = true;
				if (bNearRoad) C.Ground[T(I, J)] = G::Walk;
				else if (I < 5 || J < 5 || I > FEdge || J > FEdge) C.Ground[T(I, J)] = G::Forest;
			}

			// 64 blocks of 12x12 between the roads
			enum class EBlock : uint8 { Res, Shop, Gas, Park, Ware, Police, Clinic, Camp, Junk };
			TArray<EBlock> Kinds;
			auto Add = [&](EBlock K, int32 Count) { for (int32 I = 0; I < Count; ++I) Kinds.Add(K); };
			Add(EBlock::Res, 33); Add(EBlock::Shop, 8); Add(EBlock::Gas, 3); Add(EBlock::Park, 6); Add(EBlock::Ware, 4);
			Add(EBlock::Police, 2); Add(EBlock::Clinic, 2); Add(EBlock::Camp, 2); Add(EBlock::Junk, 4);
			for (int32 I = Kinds.Num() - 1; I > 0; --I) Kinds.Swap(I, Ri(0, I));
			int32 Bi = 0;
			for (int32 K = 0; K + 1 < Roads.Num(); ++K) for (int32 L = 0; L + 1 < Roads.Num(); ++L)
			{
				const int32 BX = Roads[L] + 3, BY = Roads[K] + 3;
				switch (Kinds[Bi++ % Kinds.Num()])
				{
				case EBlock::Res: GenRes(BX, BY); break;
				case EBlock::Shop: GenShop(BX, BY); break;
				case EBlock::Gas: GenGas(BX, BY); break;
				case EBlock::Park: GenPark(BX, BY); break;
				case EBlock::Ware: GenWare(BX, BY); break;
				case EBlock::Police: GenPolice(BX, BY); break;
				case EBlock::Clinic: GenClinic(BX, BY); break;
				case EBlock::Camp: GenCamp(BX, BY); break;
				case EBlock::Junk: GenJunk(BX, BY); break;
				}
			}

			// forest
			for (int32 J = 0; J < N; ++J) for (int32 I = 0; I < N; ++I)
			{
				if (C.Ground[T(I, J)] != G::Forest || Occ[T(I, J)]) continue;
				const bool bEdge = I < 1 || J < 1 || I > N - 2 || J > N - 2;
				if (bEdge || Chance(.42f)) AddTree(I + .5f + Rand(-.2f, .2f), J + .5f + Rand(-.2f, .2f), Chance(.6f) ? 1 : Chance(.25f) ? 2 : 0, Rand(.95f, 1.3f));
			}
			// barricades where roads leave town
			for (int32 Rd : Roads) for (int32 Ed : { 2, N - 3 }) { AddProp(P::Barrier, Rd + 1, Ed + .5f); AddProp(P::Barrier, Ed + .5f, Rd + 1, true); }

			// cars on roads, never on junctions or right next to the start
			for (int32 Count = 0, Tries = 0; Count < 100 && Tries < 9000; ++Tries)
			{
				const int32 I = Ri(3, N - 4), J = Ri(3, N - 4);
				if (C.Ground[T(I, J)] != G::Road || Occ[T(I, J)] || (RoadI[I] && RoadI[J])) continue;
				if (FVector2D::Distance(FVector2D(I, J), C.Start) < 7) continue;
				const bool bAlongY = !!RoadI[I];
				auto NearCross = [&](int32 V) { for (int32 D = 1; D <= 3; ++D) if ((V - D >= 0 && RoadI[V - D]) || (V + D < N && RoadI[V + D])) return true; return false; };
				if (bAlongY ? NearCross(J) : NearCross(I)) continue;
				if (bAlongY ? !(IsFree(I, J - 1) && IsFree(I, J + 1)) : !(IsFree(I - 1, J) && IsFree(I + 1, J))) continue;
				AddCar(I + .5f, J + .5f, bAlongY, Chance(.05f) ? ELBCarKind::Police : ELBCarKind::Sedan);
				++Count;
			}
			// street lamps
			for (int32 Rd : Roads) for (int32 K = 3; K < N - 3; K += 7)
			{
				if (RoadI[K]) continue;
				if (C.GroundAt(K, Rd - 1) == G::Walk && IsFree(K, Rd - 1)) AddLamp(K + .5f, Rd - .55f);
				if (C.GroundAt(Rd - 1, K) == G::Walk && IsFree(Rd - 1, K)) AddLamp(Rd - .55f, K + .5f);
			}
			// stop signs at junction corners
			for (int32 Rd : Roads) for (int32 Cc : Roads)
			{
				if (IsFree(Rd - 1, Cc - 1)) AddProp(P::StopSign, Rd - .7f, Cc - .7f);
				if (IsFree(Rd + 2, Cc + 2)) AddProp(P::StopSign, Rd + 2.7f, Cc + 2.7f);
			}
			// sidewalk clutter
			for (int32 Count = 0, Tries = 0; Count < 260 && Tries < 10000; ++Tries)
			{
				const int32 I = Ri(5, N - 6), J = Ri(5, N - 6);
				if (C.Ground[T(I, J)] != G::Walk || !IsFree(I, J)) continue;
				const bool bAlongX = C.GroundAt(I, J - 1) == G::Road || C.GroundAt(I, J + 1) == G::Road;
				const float Rv = R.FRand();
				const P Kind = Rv < .27f ? P::Bin : Rv < .43f ? P::Hydrant : Rv < .58f ? P::Bench : Rv < .66f ? P::NewsBox : Rv < .74f ? P::BusStop
					: Rv < .84f ? P::TrashBags : Rv < .91f ? P::Bike : Rv < .955f ? P::Cone : P::FireBarrel;
				AddProp(Kind, I + .5f, J + .5f, !bAlongX);
				++Count;
			}
		}
	};
}

void FLBCity::Generate(int32 Seed)
{
	*this = FLBCity();
	FGen Gen(*this, Seed);
	Gen.Run();
}

#undef LOCTEXT_NAMESPACE
