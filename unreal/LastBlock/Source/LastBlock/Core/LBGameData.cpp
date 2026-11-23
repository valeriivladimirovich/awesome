#include "Core/LBGameData.h"

#define LOCTEXT_NAMESPACE "LastBlock"

namespace
{
	FLBWeaponDef MakeWeapon(const FText& Name, float Dmg, float Range, float Cd, float Sink, int32 Rank)
	{
		FLBWeaponDef W; W.Name = Name; W.Damage = Dmg; W.Range = Range; W.Cooldown = Cd; W.SinkDistance = Sink; W.Rank = Rank;
		return W;
	}
	FLBWeaponDef MakeGun(const FText& Name, float Dmg, float Range, float Cd, int32 Mag, ELBItem Ammo, float Noise, float Spread, float Reload, bool bAuto, int32 Rank)
	{
		FLBWeaponDef W; W.Name = Name; W.Damage = Dmg; W.Range = Range; W.Cooldown = Cd; W.bGun = true; W.bAuto = bAuto;
		W.MagSize = Mag; W.Ammo = Ammo; W.Noise = Noise; W.Spread = Spread; W.ReloadTime = Reload; W.Rank = Rank;
		return W;
	}
	FLBFurnitureDef MakeFurn(float W, float H, float HeightCm, const TCHAR* Hex, bool bCenter = false, const FText& Cont = FText::GetEmpty(), FName Loot = NAME_None)
	{
		FLBFurnitureDef F; F.Size = FVector2D(W, H); F.HeightCm = HeightCm; F.Color = FLinearColor::FromSRGBColor(FColor::FromHex(Hex));
		F.bCentered = bCenter; F.ContainerName = Cont; F.LootTable = Loot;
		return F;
	}

	struct FLootRow { ELBItem Item; float Weight; };
	using FLootTable = TArray<FLootRow>;

	const TMap<FName, FLootTable>& LootTables()
	{
		using I = ELBItem;
		static const TMap<FName, FLootTable> T = {
			{ "fridge",    { {I::Food, 5}, {I::Water, 5} } },
			{ "counter",   { {I::Food, 4}, {I::Water, 3}, {I::Battery, 1} } },
			{ "wardrobe",  { {I::Bandage, 2}, {I::Battery, 2}, {I::Food, 1}, {I::Bat, .4f}, {I::Ammo9, .5f}, {I::Pistol, .12f} } },
			{ "cabinet",   { {I::Bandage, 5}, {I::Medkit, 1} } },
			{ "desk",      { {I::Battery, 4}, {I::Bandage, 1}, {I::Ammo9, .6f} } },
			{ "crate",     { {I::Food, 2}, {I::Water, 2}, {I::Battery, 2}, {I::Bat, .8f}, {I::Axe, .5f}, {I::Ammo9, .5f} } },
			{ "workbench", { {I::Bat, 3}, {I::Axe, 2}, {I::Battery, 2} } },
			{ "locker",    { {I::Pistol, 1.2f}, {I::Ammo9, 3}, {I::Rifle, .4f}, {I::Ammo545, 1.2f}, {I::Bandage, 1}, {I::Bat, 1} } },
			{ "military",  { {I::Rifle, 1}, {I::Ammo545, 3}, {I::Ammo9, 1.5f}, {I::Pistol, .6f}, {I::Medkit, 1.2f}, {I::Food, 1}, {I::Water, 1} } },
			{ "shop",      { {I::Food, 6}, {I::Water, 5} } },
			{ "pharm",     { {I::Bandage, 5}, {I::Medkit, 1.5f} } },
			{ "hard",      { {I::Battery, 4}, {I::Bat, 2}, {I::Axe, 1.5f} } },
			{ "cafe",      { {I::Food, 5}, {I::Water, 4} } },
		};
		return T;
	}
}

FLBWeaponDef ULBGameData::GetWeapon(ELBWeapon Weapon)
{
	static const TArray<FLBWeaponDef> Table = {
		MakeWeapon(LOCTEXT("Fists", "Кулаки"), 1.f, .8f, .42f, .40f, 0),
		MakeWeapon(LOCTEXT("Bat", "Бита"), 2.f, 1.f, .5f, .74f, 1),
		MakeWeapon(LOCTEXT("Axe", "Топор"), 3.2f, 1.05f, .6f, .76f, 2),
		MakeGun(LOCTEXT("Pistol", "Пистолет"), 2.3f, 9.f, .3f, 12, ELBItem::Ammo9, 15.f, .05f, 1.3f, false, 3),
		MakeGun(LOCTEXT("Rifle", "Автомат"), 2.1f, 11.f, .11f, 30, ELBItem::Ammo545, 20.f, .09f, 1.8f, true, 4),
	};
	return Table[FMath::Clamp((int32)Weapon, 0, Table.Num() - 1)];
}

FLBFurnitureDef ULBGameData::GetFurniture(ELBFurniture Kind)
{
	using F = ELBFurniture;
	static const TMap<F, FLBFurnitureDef> Table = {
		{ F::Bed,           MakeFurn(.9f, 1.15f, 60, TEXT("6a4b38")) },
		{ F::HospitalBed,   MakeFurn(.6f, 1.1f, 70, TEXT("b8bcc4")) },
		{ F::Sofa,          MakeFurn(1.2f, .5f, 90, TEXT("5b6b8a")) },
		{ F::TV,            MakeFurn(.7f, .28f, 124, TEXT("4a3a2e")) },
		{ F::Table,         MakeFurn(.7f, .7f, 77, TEXT("8a6040"), true) },
		{ F::CoffeeTable,   MakeFurn(.45f, .45f, 75, TEXT("6e4a32"), true) },
		{ F::Wardrobe,      MakeFurn(.8f, .35f, 206, TEXT("7a5638"), false, LOCTEXT("Wardrobe", "Шкаф"), "wardrobe") },
		{ F::Fridge,        MakeFurn(.42f, .4f, 185, TEXT("e3e3df"), false, LOCTEXT("Fridge", "Холодильник"), "fridge") },
		{ F::Counter,       MakeFurn(.7f, .36f, 90, TEXT("cfc6b4"), false, LOCTEXT("Counter", "Кухонный шкаф"), "counter") },
		{ F::Stove,         MakeFurn(.42f, .36f, 91, TEXT("d8d8d4")) },
		{ F::Bath,          MakeFurn(.95f, .45f, 56, TEXT("efefea")) },
		{ F::Toilet,        MakeFurn(.25f, .35f, 84, TEXT("efefea")) },
		{ F::Cabinet,       MakeFurn(.5f, .22f, 178, TEXT("e6e2d8"), false, LOCTEXT("Cabinet", "Шкафчик"), "cabinet") },
		{ F::Desk,          MakeFurn(.8f, .42f, 76, TEXT("7d5a3c"), false, LOCTEXT("Desk", "Стол"), "desk") },
		{ F::Bookshelf,     MakeFurn(.7f, .22f, 195, TEXT("6d4c33")) },
		{ F::Plant,         MakeFurn(.25f, .25f, 75, TEXT("9a6040")) },
		{ F::Shelf,         MakeFurn(1.3f, .36f, 170, TEXT("8e959e"), true, LOCTEXT("Shelf", "Полка"), "shop") },
		{ F::Checkout,      MakeFurn(.95f, .45f, 90, TEXT("b8b0a0"), false, LOCTEXT("Checkout", "Касса"), "counter") },
		{ F::Crate,         MakeFurn(.55f, .55f, 70, TEXT("9a7a4c"), false, LOCTEXT("Crate", "Ящик"), "crate") },
		{ F::Workbench,     MakeFurn(1.f, .42f, 80, TEXT("7a5a3c"), false, LOCTEXT("Workbench", "Верстак"), "workbench") },
		{ F::Locker,        MakeFurn(.7f, .28f, 190, TEXT("5e6b78"), false, LOCTEXT("Locker", "Шкафчик"), "locker") },
		{ F::MilitaryCrate, MakeFurn(.6f, .36f, 55, TEXT("4f5a3a"), false, LOCTEXT("MCrate", "Армейский ящик"), "military") },
	};
	static const FLBFurnitureDef Fallback;
	const FLBFurnitureDef* Found = Table.Find(Kind);
	return Found ? *Found : Fallback;
}

TArray<ELBItem> ULBGameData::RollLoot(FName Table, int32 RichMax, FRandomStream& Stream)
{
	const FLootTable* T = LootTables().Find(Table);
	if (!T) T = LootTables().Find("crate");
	float Total = 0.f;
	for (const FLootRow& R : *T) Total += R.Weight;

	TArray<ELBItem> Out;
	int32 Count = Stream.FRand() < (RichMax >= 3 ? .35f : .5f) ? 0 : 1;
	if (Count && Stream.FRand() < .2f) Count = 2;
	for (; Count > 0; --Count)
	{
		float Roll = Stream.FRand() * Total;
		for (const FLootRow& R : *T)
		{
			Roll -= R.Weight;
			if (Roll <= 0.f) { Out.Add(R.Item); break; }
		}
	}
	return Out;
}

FText ULBGameData::GetItemName(ELBItem Item)
{
	switch (Item)
	{
	case ELBItem::Food: return LOCTEXT("ItemFood", "Консервы");
	case ELBItem::Water: return LOCTEXT("ItemWater", "Вода");
	case ELBItem::Bandage: return LOCTEXT("ItemBandage", "Бинт");
	case ELBItem::Medkit: return LOCTEXT("ItemMedkit", "Аптечка");
	case ELBItem::Battery: return LOCTEXT("ItemBattery", "Батарейка");
	case ELBItem::Ammo9: return LOCTEXT("ItemAmmo9", "Патроны 9 мм");
	case ELBItem::Ammo545: return LOCTEXT("ItemAmmo545", "Патроны 5,45");
	case ELBItem::Bat: return GetWeapon(ELBWeapon::Bat).Name;
	case ELBItem::Axe: return GetWeapon(ELBWeapon::Axe).Name;
	case ELBItem::Pistol: return GetWeapon(ELBWeapon::Pistol).Name;
	case ELBItem::Rifle: return GetWeapon(ELBWeapon::Rifle).Name;
	default: return FText::GetEmpty();
	}
}

bool ULBGameData::ItemToWeapon(ELBItem Item, ELBWeapon& OutWeapon)
{
	switch (Item)
	{
	case ELBItem::Bat: OutWeapon = ELBWeapon::Bat; return true;
	case ELBItem::Axe: OutWeapon = ELBWeapon::Axe; return true;
	case ELBItem::Pistol: OutWeapon = ELBWeapon::Pistol; return true;
	case ELBItem::Rifle: OutWeapon = ELBWeapon::Rifle; return true;
	default: OutWeapon = ELBWeapon::Fists; return false;
	}
}

const TArray<ELBFurniture>& ULBGameData::RoomSet(FName Room)
{
	using F = ELBFurniture;
	static const TMap<FName, TArray<F>> Sets = {
		{ "living",  { F::Sofa, F::TV, F::Bookshelf, F::Plant, F::Table } },
		{ "livkit",  { F::Sofa, F::TV, F::Fridge, F::Counter, F::Stove, F::Plant } },
		{ "kitchen", { F::Fridge, F::Counter, F::Stove, F::Counter, F::Table } },
		{ "bedroom", { F::Bed, F::Wardrobe, F::Desk, F::Plant } },
		{ "bath",    { F::Bath, F::Toilet, F::Cabinet } },
		{ "studio",  { F::Bed, F::Fridge, F::Counter, F::Wardrobe, F::Sofa } },
	};
	static const TArray<F> Empty;
	const TArray<F>* Found = Sets.Find(Room);
	return Found ? *Found : Empty;
}

#undef LOCTEXT_NAMESPACE
