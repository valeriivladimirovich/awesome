# Последний квартал — Unreal Engine 5.8

Перенос браузерной игры [`mobile-games/kvartal`](../../mobile-games/kvartal/) на Unreal Engine 5.8 под iPhone
(A17 Pro и новее), альбомная ориентация, 60 кадров.

Логика перенесена из браузерной версии один в один: та же карта 150×150 клеток по 1,8 м, те же таблицы
лута, оружия и мебели, те же скорости, зрение и слух зомби. Графика — Unreal.

> **Состояние.** Код написан, но ни разу не собирался в Unreal: в среде, где он писался, движка нет.
> Генератор города отдельно собран обычным компилятором C++ и запущен: на трёх сидах он даёт около
> 167 зданий, примерно 1000 предметов мебели, 730 контейнеров, 166 машин и 340 фонарей, вся мебель
> стоит внутри своих зданий. При первой сборке в Unreal возможны мелкие ошибки компиляции — их нужно
> будет поправить по логу.

## Что внутри

| Папка | Что делает |
|---|---|
| `Core/` | Типы, таблицы оружия, лута и мебели (`ULBGameData`), настройки проекта (`ULBSettings`) |
| `World/LBCityGen` | Генератор города без UObject: дороги, 64 квартала (дома с комнатами, магазины, аптеки, кафе, АЗС, склады, полиция, больницы, военные блокпосты, свалки, парки), лес, машины, фонари, уличные предметы |
| `World/LBCity` | Превращает данные в меши: по одному HISM на тип детали (земля, стены, окна, двери, заборы, деревья, мебель, предметы), крыша дома прячется, когда игрок внутри; пул из 6 источников света для фонарей; поиск контейнеров с проверкой прямой видимости |
| `World/LBVehicle` | Припаркованные машины: кузов и 4 колеса отдельными компонентами (задел под езду), мигалки у полиции и скорой, дым у сгоревших |
| `World/LBDayNight` | Часы, солнце, луна, туман, дождь, мокрые улицы; параметры `Wetness`, `Night`, `PlayerPos` в Material Parameter Collection |
| `Player/LBSurvivor` | Игрок: ходьба и бег от стика, изометрическая камера, голод, жажда, здоровье, батарейка фонарика, обыск, рюкзак, оружие |
| `Zombies/` | Зомби и их контроллер: слух через AI Perception, зрение вручную (ночь, дождь, фонарик, фонари), погоня, поиск, блуждание |
| `AI/` | Узлы Behavior Tree на C++: `LB Zombie Senses`, `LB Zombie Attack`, `LB Pick Wander Point` |
| `FX/LBFX` | Эффекты Niagara: кровь, искры от попаданий, трассеры, вспышка выстрела, пыль; звуки |
| `Game/` | Режим игры (сам создаёт город, день и ночь, расселяет зомби, рестарт после смерти) и контроллер с экранным джойстиком |
| `UI/LBHUDWidget` | Интерфейс целиком на C++: часы, полоски здоровья, еды, воды, батарейки, оружие и патроны, сообщения, кнопки «Удар», «Обыскать», «Перезарядка», «Оружие», «Рюкзак» |

### Бой, как в последней браузерной версии

- Удар не отталкивает зомби. Персонаж делает шаг, кулак или оружие входят в тело, урон засчитывается
  в момент касания (на 0,13 с замаха). Зомби вздрагивает на месте: `HitReact` и `HitAngle` для
  Animation Blueprint, шаг сбивается, его замах отменяется.
- Смерть — настоящий физический рэгдолл из Physics Asset. Топор может отрубить руку при жизни
  (28 %) и конечность или голову при смерти; выстрел в голову её сносит (`HideBoneByName`).
- Огнестрел: доводка на ближайшего зомби, разброс, магазины, перезарядка, стены останавливают пулю,
  выстрел слышно на 15–20 клеток.

## Как применены скиллы

- **C++ геймплей**: все классы на рефлексии UE (`UCLASS`, `UPROPERTY`, `UFUNCTION`), GC-safe указатели
  (`TObjectPtr`, `TWeakObjectPtr`, `TSoftObjectPtr` для ассетов), делегаты для интерфейса (`OnToast`,
  `OnStatsChanged`, `OnDied`), таймеры для населения и рестарта, `IsValid`, без `delete` у UObject.
- **Behavior Trees / AI**: AI Perception (слух) плюс свои узлы дерева; Blackboard-ключи `State` (Enum),
  `Target` (Object), `MoveGoal` (Vector). Пока дерева нет, та же логика работает в контроллере, так что
  игра запускается сразу. Далёкие зомби (дальше 30 клеток) думают раз в секунду.
- **Niagara**: эффекты спавнятся через пул (`ENCPoolMethod::AutoRelease`), управляются User-параметрами
  (`User.Direction`, `User.Normal`, `User.BeamEnd`, `User.Intensity`), в README ниже — требования к Fixed
  Bounds и бюджету частиц.
- **Packaging**: `Scripts/build-ios.sh` — BuildCookRun под iOS, Shipping, IoStore, сжатие; настройки
  упаковки в `Config/DefaultGame.ini`.
- **Blueprints**: всё, что удобно настраивать в редакторе, открыто как `EditDefaultsOnly` и `BlueprintReadOnly`:
  меши, монтажи анимаций, оружие в руке, класс зомби.

## Что нужно

- Mac с Apple Silicon, Xcode (последний), Unreal Engine 5.8 из Epic Games Launcher.
- Для установки на iPhone — аккаунт Apple Developer (бесплатного хватает для своего телефона на 7 дней).

## Первый запуск (один раз, примерно 15 минут)

1. Скомпилировать модуль и открыть проект:
   ```bash
   cd unreal/LastBlock
   UE_ROOT="/Users/Shared/Epic Games/UE_5.8" ./Scripts/build-editor-mac.sh
   open LastBlock.uproject
   ```
   Или просто двойной клик по `LastBlock.uproject` — редактор сам предложит собрать модуль.
2. Создать уровень: **File → New Level → Empty Level**, сохранить как `Content/Maps/L_Town`.
3. Поставить в уровень **Nav Mesh Bounds Volume** (Place Actors → Volumes) и задать
   Location `(13500, 13500, 0)`, Scale `(140, 140, 10)` — он покрывает весь город 270×270 м.
   Навигация строится во время игры, потому что город генерируется при старте.
4. Нажать **Play**. Город, солнце и зомби появятся сами (`ALBGameMode` создаёт всё, чего нет на уровне).
   Пока не подключены модели, вместо них цветные блоки — это нормально.

## Подключение графики (по желанию, по одному)

Всё делается в **Project Settings → Game → Last Block**, без правки кода.

- **M_Tint** — материал для всех блоков: `Base Color = PerInstanceCustomData(0..2)` (узел
  `PerInstanceCustomData` три раза, индексы 0, 1, 2, собрать в вектор), плюс параметр `Color` (Vector)
  для крыш и машин. Указать в `Tint Material`.
- **MPC_World** — Material Parameter Collection со скалярами `Wetness`, `Night` и вектором `PlayerPos`.
  В материале асфальта: `Roughness = lerp(0.9, 0.15, Wetness)` и лужи по маске шума. В материале стен:
  срезать стены между камерой и игроком (`SphereMask` вокруг `PlayerPos` + `DitherTemporalAA` в Opacity Mask).
- **Модели**: стены (1 клетка = 180 см в ширину, 300 см в высоту, пивот снизу по центру, лицом по +X),
  мебель по именам (`Sofa`, `Bed`, `Fridge`…), машины по типам (`Sedan`, `Hatch`, `SUV`, `Pickup`,
  `Police`, `Truck`, `Ambulance`, `MilitaryTruck`), колесо, деревья, фонарь.
- **Персонажи**: создать Blueprint-наследников `ALBSurvivor` и `ALBZombie`, выставить Skeletal Mesh
  (например, UE5 Manny/Quinn или свой), Physics Asset для рэгдолла и Animation Blueprint. В AnimBP
  зомби читать `HitReact`, `HitAngle`, `Limp`, `bRunner`, `AttackWindup`; у игрока — `SwingAlpha`,
  `SearchLeft`. Затем создать Blueprint-наследника `ALBGameMode`, указать в нём `Default Pawn Class`
  (Blueprint игрока) и `Zombie Class` (Blueprint зомби) и выбрать его в **Project Settings → Maps & Modes →
  Default GameMode**.

## Эффекты Niagara (по желанию)

Создать системы и указать их в настройках. Для всех: **Fixed Bounds**, ограничение `Max Particles`,
CPU-симуляция для коротких вспышек и GPU для дождя.

| Слот | Что сделать | User-параметры |
|---|---|---|
| `BloodFX` | Burst 6–14 капель, гравитация, Collision → декаль на земле | `User.Direction` (Vector), `User.Amount` (Int) |
| `ImpactFX` | 5 искр (аддитивные, 0,18 с) + облачко пыли | `User.Normal` (Vector) |
| `TracerFX` | Ribbon или Beam от точки спавна до конца, 0,07 с | `User.BeamEnd` (Vector) |
| `MuzzleFX` | Вспышка 0,05 с + Light Renderer | — |
| `SmokeFX` | Столб дыма, Spawn Rate 6, жизнь 4 с, Curl Noise | — |
| `DustFX` | 2–3 спрайта пыли у ног | — |
| `RainFX` | GPU, капли в кубе 30×30 м вокруг игрока | `User.Intensity` (Float 0..1) |

## Дерево поведения зомби (по желанию)

Без дерева зомби работают на той же логике в C++. Чтобы вести её в редакторе:

1. **BB_Zombie** (Blackboard): `State` — Enum `ELBZombieState`, `Target` — Object (Actor), `MoveGoal` — Vector.
2. **BT_Zombie**:
   ```
   Root
   └── Selector                       [Service: LB Zombie Senses]
       ├── Sequence                   [Blackboard: State == Chase]
       │   ├── Move To (Target, Acceptance 90)
       │   └── LB Zombie Attack
       ├── Sequence                   [Blackboard: State == Investigate]
       │   └── Move To (MoveGoal)
       └── Sequence                   [Blackboard: State == Wander]
           ├── LB Pick Wander Point
           ├── Move To (MoveGoal)
           └── Wait (4 ± 2 с)
   ```
   У декораторов поставить Observer Aborts = **Both**, чтобы погоня прерывала блуждание.
3. Указать `BT_Zombie` в `Zombie Tree`.

Отладка: в игре консоль `ShowDebug AI`, `ShowDebug BehaviorTree`.

## Сборка для iPhone

1. **Project Settings → Platforms → iOS**: свой Bundle Identifier (вместо `com.example.lastblock`),
   команда Apple Developer, автоматическая подпись.
2. Собрать:
   ```bash
   UE_ROOT="/Users/Shared/Epic Games/UE_5.8" ./Scripts/build-ios.sh
   # для отладки с логами: CONFIG=Development ./Scripts/build-ios.sh
   ```
3. Установить `.ipa` из `Build/IOS-Shipping` через Xcode → Window → Devices and Simulators.

Логи сборки: `~/Library/Logs/Unreal Engine/LocalBuildLogs/`. Если упало — листать вверх до первой `ERROR`.

## Настройки под iPhone

- Мобильный deferred-рендер (`r.Mobile.ShadingPath=1`) — много динамических источников света
  (фонари, фонарик, мигалки), HDR, GPU Scene для инстансов, TAA.
- `Config/DefaultDeviceProfiles.ini`: для A17 Pro разрешение рендера ×2,6 от точек экрана, тени 2048, 2 каскада.
- 60 кадров (`FrameRateLock=PUFRL_60`), только альбомная ориентация.
- Город — около 30 HISM-компонентов вместо десятков тысяч отдельных объектов; дальние инстансы отсекаются
  с 90 м; фонари светят пулом из 6 ближайших.
