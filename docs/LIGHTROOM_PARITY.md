# Lightroom Classic 14.4 parity backlog

The first Windows build of Apertone 2.5 uses the repository's Qt desktop app,
SQLite catalog and native RAW engine. The target workflow is a complete local
photo session from an SD card or folder through culling, Develop and delivery.
Priorities below put safe migration and dependable editing ahead of secondary
publishing modules.

## Reference installation

- The Windows uninstall record and executable version identify **Lightroom
  Classic 14.4**, build `202506051112-5918896a`. The installation exposes
  Library, Develop, Import, Export, Layout, Location, Multiple Monitor, Book,
  Slideshow, Print and Web modules. The June 2025 release integrated Denoise,
  Raw Details and Super Resolution into Detail and added a consolidated AI edit
  status. [Adobe 14.4 release details](https://helpx.adobe.com/lightroom-classic/desktop/help/whats-new/2025-4.html)
- The local catalog is a SQLite database. Read-only schema inspection found
  127 tables, including file and folder references, image variants, develop
  settings, history, snapshots, EXIF/IPTC, keyword relationships, collections
  and stacks. Preview and Smart Preview stores sit beside the catalog.
  [Adobe catalog documentation](https://helpx.adobe.com/lightroom-classic/desktop/manage-catalogs-and-files/create-catalogs.html)
  [Smart Previews](https://helpx.adobe.com/uk/lightroom-classic/desktop/viewing-photos/lightroom-smart-previews.html)
- The installed `Resources/Settings` contains 959 XMP files with 559 unique
  `crs` keys. Of these, 324 are dynamically named `Table_<digest>` payload
  references. The other 235 names include settings, preset metadata, masks and
  profile references. The installation also contains 4,100 DCP camera profile
  files and 3,248 LCP lens profile files. These counts describe the installation
  inventory. [Adobe Camera Raw XMP namespace](https://developer.adobe.com/xmp/docs/xmp-namespaces/crs/)
- Catalog state, sidecars, masks, versions and previews establish the
  observable data model. Adobe's internal processing order and numerical
  algorithms remain unverified. Public controls and repeatable output
  comparisons define the acceptance boundary.

## Daily editing and organization

| Lightroom workflow | Apertone 2.5 and Windows scope | Next acceptance check |
| --- | --- | --- |
| Import from cards and folders, Add/Copy/Move, duplicate detection, renaming, destination folders, second copy, presets and metadata | Shared import journal, hashes, naming and metadata presets, source refresh and relinking. Windows uses wide paths and same-volume publication. A DCIM picker opens mounted cards and the capture watcher monitors a local folder. Windows can trigger and import a still photo from a camera with a compatible WPD driver. | Qualify direct capture with supported camera hardware and power loss on a physical exFAT SD card. The Windows workflow also exercises native exFAT using an ephemeral VHD. [Windows still-image capture](https://learn.microsoft.com/en-us/windows/win32/wpd_sdk/wpd-command-still-image-capture-initiate-command) |
| Grid, Loupe, filmstrip, Compare, Survey, second monitor, flags, labels, stars and keyboard navigation | Library and Develop, linked comparisons and culling, metadata filters, named collections and an independent review window. Survey reviews 2–12 photos with their Develop edits and supports removing individual photos from the selection. Windows culling uses sharpness and exposure metrics. | Verify range selection, keyboard focus, zoom synchronization and selection restoration on a multi-display system. [Adobe Library workflow](https://helpx.adobe.com/lightroom-classic/desktop/help/library-module-basic-workflow.html) |
| Folders, regular and Smart Collections, nested keywords, face groups, stacks and virtual copies | Manage manual stacks and covers, nested collection sets, regular and Smart Collections, keyword paths and aliases. Independently editable virtual copies share the original file on disk. | Qualify keyword alias and collection-set behavior at catalog scale. Continue camera and face-group compatibility checks. [Adobe stacking](https://helpx.adobe.com/lightroom-classic/desktop/organize-photos-in-lightroom-classic/grouping-photos-stacks.html) |
| Develop with the original offline using a Smart Preview | Opening a photo in Develop caches up to 1600 px of linear image data and its RAW calibration, keyed by the source digest and cache format version. Saved edits survive offline restarts. Full-resolution export requires the original. | Verify a physical SD or external drive disconnect and reconnection, constrained disk space, cache recovery and batch preview creation. [Adobe Smart Previews](https://helpx.adobe.com/uk/lightroom-classic/desktop/viewing-photos/lightroom-smart-previews.html) |
| Before/After, reference image, snapshots, history, copy/paste, sync and saved presets | Linked Before/After, pinned reference, persisted history, named versions, Quick Looks and atomic batch Apply/Paste/Sync. | Qualify reusable Copy/Paste subsets and one-step undo across larger batches. [Adobe Develop](https://helpx.adobe.com/lightroom-classic/desktop/process-and-develop-photos/develop-module-tools.html) |
| RAW profiles, WB, exposure, tone, Presence, RGB curves, HSL, Point Color, grading, B&W, calibration | LibRaw, embedded DNG calibration, single- and dual-illuminant DCP camera profiles, integer Bayer, X-Trans, monochrome RAW, scene-linear working pixels, eight-channel B&W and primary color calibration. The Lightroom importer can match selected DCP files to RAW photos. | Compare neutral patches, highlight ramps and extreme WB across cameras. Extend profile support to three-illuminant calibration and XMP looks. [Adobe profiles and tone](https://helpx.adobe.com/ca/lightroom-classic/desktop/process-and-develop-photos/image-tone-color.html) |
| Crop, straighten, lens profiles, defringe, perspective and Upright | Shared crop, calibrated lens correction, manual/Auto/Guided transform and purple/green defringe. | Broaden optical calibration. Measure residual distortion and color fringing for measured lens/focal-length pairs. |
| Sharpening, noise, Denoise, Raw Details, Super Resolution, HDR editing and Lens Blur | Shared manual Detail and deterministic Detail DNG operations. Windows uses local model inference for denoise and relative depth, with denoise provenance persisted. | Qualify skin, fine texture, dark-frame artifacts and cancellation across hardware. Continue optical depth and Lens Blur comparisons. [Adobe 14.4 release details](https://helpx.adobe.com/lightroom-classic/desktop/help/whats-new/2025-4.html) |
| Local brush, gradients, color/luminance/depth range, AI subjects, objects, people, sky and composite masks | Brush and range controls, mask composition and overlays. Windows models provide local subject, background, clicked object, sky, people, facial regions and relative depth masks. Apertone imports saved Lightroom grayscale masks and compatible linear gradients in source coordinates. | Qualify difficult boundaries, extend geometry interchange to brush and range masks, and check historical mask states. [Adobe masking](https://helpx.adobe.com/lightroom-classic/desktop/process-and-develop-photos/masking.html) |
| Heal/Clone, red eye, content-aware and generative removal | Shared retouch, local Remove and localized red-eye correction. | Qualify clone and heal edge cases and assess removal against small and large occlusions. |
| HDR and panorama merge | Shared full-resolution merge and derived DNG. Windows has verified translation registration for alignment. | Test bracketed high-contrast images and panorama yaw/parallax. Add a more general registration provider when the translation model has insufficient overlap. |
| Export with resize, color space, output sharpening, metadata, watermarks, naming and saved recipes | Shared export queue and recipes. Windows writes JPEG, PNG, TIFF and processed DNG, with metadata policy controls for standard formats, 8/16-bit ZIP TIFF, and sRGB Ultra HDR JPEG with an SDR base and a gain map. | Qualify color tags, sharpening, cancellation, large exports and HDR viewing across readers and displays. [Adobe export settings](https://helpx.adobe.com/lightroom-classic/desktop/export-photos/export-files-disk-or-cd.html) [Ultra HDR image format](https://developer.android.com/media/platform/hdr-image-format) |
| Map, Book, Slideshow, Print, Web and Publish Services | Offline EXIF map, ICC soft proof, contact sheets and photo books in PDF, local HTML gallery and slideshow, and one-time publication of HTML galleries to WebDAV. | Qualify physical print calibration and real WebDAV hosting. Scope synchronization for galleries that change after publication. [Adobe module comparison](https://helpx.adobe.com/uk/lightroom-classic/desktop/kb/lightroom-feature-comparison.html) |

## Migration and settings coverage

| XMP and catalog evidence | Implementation backlog |
| --- | --- |
| `Version`, `ProcessVersion`, `Supports*`, `CameraModelRestriction`, `PresetType`, `UUID` | Track source version and compatibility at import. Preserve absent versus explicit neutral fields. Reject unknown mandatory constructs with an actionable report. |
| `Exposure2012`, `Highlights2012`, `Shadows2012`, `WhiteBalance`, `Temperature`, `Tint`, Presence, HSL, parametric and RGB curve arrays | Apply a selected XMP directly to the active photo from Presets → User → Actions. The sparse parser reports unsupported fields and creates one undo step. Add import preview and compare legacy and modern Process Versions on controlled images. |
| `CameraProfile`, `ProfileGainTableMap`, `Look`, `RGBTables`, `LensProfileEnable`, optical and perspective fields | Add explicit compatible mappings for independently authored or user-supplied profiles. Verify a neutral scale and preserve unknown fields on round trips. |
| `ColorGrade*`, `SplitToning*`, `GrayMixer*`, `PointColors`, `PostCropVignette*`, `Grain*`, `Sharpen*`, `LuminanceNoiseReduction*` | Sparse import maps supported grading, split toning, vignette, grain, sharpening and noise-reduction values. Verify numerical and photographic differences and add compatible B&W mixer and Point Color mappings. |
| `MaskGroupBasedCorrections`, `CorrectionMasks`, `MaskBlendMode`, `MaskInverted`, `Local*`, `LensBlur` | Start with geometric masks, numeric corrections and compositing; transform source coordinates against orientation and crop. Report AI mask and profile references that need a local substitute. |
| `AgLibraryFile` and `Adobe_images` relation, `masterImage`, separate Develop state, history and collections | The `.lrcat` importer transfers supported settings, history, snapshots, metadata, independently editable copies, nested sets and keyword aliases. It converts supported Smart Collection rules and retains source rule text. Qualify additional Lightroom catalog versions and remaining Adobe-specific settings. |

For migration, use synthetic files and approved exported XMP fixtures. Verify
byte-identical originals, identical second-import counts, rollback after an
interruption, preserved unknown settings and a per-image mapping report.
Consider `.acr` sidecars in the separate Lightroom 15+ compatibility block.
[Adobe catalog and XMP behavior](https://helpx.adobe.com/lightroom-classic/desktop/organize-photos-in-lightroom-classic/metadata-basics-actions.html)

## Implementation order within one Windows release

All four blocks belong to the same Windows release. The release gate covers the
complete workflow, migration and backward compatibility, image fidelity, recovery
and installer acceptance. The final GitHub artifact and installation on the
target drive follow release validation.

1. **Windows daily-use foundation.** Build and test the desktop app and CLI, RAW
   and raster open, Unicode catalogs and originals, persisted edits, copy import,
   and PNG/JPEG/TIFF/DNG exports. Follow with long-running catalog and removable
   drive qualification, GPU preview and Windows-specific model providers.
2. **Migration and file compatibility.** Extend per-photo XMP Develop, then add
   catalog migration, editable proxies, virtual copies, stacks and collection
   parity.
3. **Rendering and interaction.** Compare SDR and HDR output on controlled RAW,
   add color and optics controls, local model providers, precise registration,
   extra review views and proofing. Set performance budgets from timed imports,
   slider feedback, preview switching and large batch exports on Windows.
4. **Delivery modules.** Expand metadata, HDR and output formats. Add print,
   map and publishing workflows with separate acceptance fixtures.

### Local verification

- The Windows 2.5.0 build passed all seven local CTests. The acceptance
  run used NEF and JPEG originals from a separate physical NTFS drive and an
  isolated test catalog. It covered stack membership and cover changes,
  truncated preview recovery, monochrome export, contact sheet, photo book,
  gallery and slideshow delivery. Original SHA-256 digests matched.
- Windows face, eyes and lips masks persisted on a JPEG. Estimated depth
  masks persisted on a JPEG and a Nikon NEF, and the PNG re-export after
  restarting matched the first export byte for byte.
- The four metadata policies were exercised on SDR JPEG, PNG, ZIP TIFF and
  Linear DNG with an EXIF GPS fixture. DRUNet produced an independently
  registered Linear DNG from a 1,024-pixel derivative of a Nikon NEF.
- NTFS Copy imported 1,000 distinct JPEG files and matched every source and
  destination SHA-256. A Fujifilm X-T4 RAF and a Leica M Monochrom (Typ 246)
  DNG opened and exported. [CC0 RAW test repository](https://raw.pixls.us/)
- The Windows release includes manual stacks, pinned references, a separate
  review window, bulk offline preview preparation, DCIM card selection,
  capture-folder monitoring, collection-set and keyword editors, imported
  Develop history and named snapshots, and WebDAV gallery output.
- A read-only sample of the Lightroom 14.4 catalog and two native Nikon NEFs
  produced six Develop history entries, each with a partial settings mapping.
  The current steps, set hierarchy and repeat import were checked. The catalog
  and image hashes matched before and after. Snapshot fixtures cover two
  states with the same name, a copy-specific state and an invalid payload.
- A local WebDAV fixture checks Basic authentication, Unicode destination
  names, the HTML/JPEG payload, failure cleanup, collision protection and
  redirect rejection. [WebDAV MOVE and Overwrite semantics](https://www.rfc-editor.org/rfc/rfc4918.html#section-9.9)
- The synthetic Lightroom fixture covers positional bitmap and gradient
  masks, DCP selection and rendering after restart, missing mask payloads,
  preserved local edits and repeat import without duplicate versions.
- Remaining device checks include a tethered shutter release with a real
  camera, interruption on a physical exFAT card and paper print calibration.
  Compressed or missing mask payloads and unsupported Lightroom geometries
  remain visible in the migration report.

- The synthetic Lightroom fixture covers a committed WAL, Unicode source
  paths, interpreted Develop settings, IPTC, manual collections, independent
  virtual copies, dynamic rules and idempotence after copying the source catalog.
  It checks hashes of the original image, catalog and WAL.
- The Windows daily-workflow fixture covers a separate-process offline edit
  and reconnect, byte-identical PNG after reconnect, TIFF ZIP depth, alpha
  and ICC tags, reopen through the TIFF reader and Survey selection.
- The Ultra HDR fixture checks SDR JPEG readability, ISO and XMP gain-map
  metadata, and highlight recovery through the libultrahdr decoder with
  even and odd output dimensions.
- A staged native Windows package opens a synthetic DNG, exports Ultra HDR
  JPEG, starts the desktop without Qt or MinGW on PATH, and exposes its icon
  through Windows Shell.
- Seven local Windows scenarios used real JPEGs and Nikon NEFs. These exercised
  the five mask modes, persistence, unchanged source hashes, independent copy
  edits and rating, and full-resolution ZIP TIFF and Ultra HDR JPEG export.
- Remaining release qualification includes native interaction and display
  checks, photographic comparisons, 1,000-file removable-drive imports,
  catalog load and recovery, and verification of the final GitHub package.

## Observed preset fields

The following Camera Raw keys are present in the installed 14.4 settings
files. The number in parentheses is the count of files containing that key.
The 324 digest-keyed `Table_*` references are counted in the installation
summary. Per-photo masks, crop and other settings can add keys outside the
preset inventory.

```text
Active (13)                              Amount (54)                              AutoGrayscaleMix (7)
AutoLateralCA (12)                       AutoTone (12)                            Baseline (10)
Blacks2012 (313)                         BlueCurve (3)                            BlueHue (131)
BlueSaturation (131)                     BlurAmount (13)                          BokehAspect (13)
BokehRotation (13)                       BokehShape (13)                          BokehShapeDetail (13)
Brightness (1)                           CameraModelRestriction (959)             CameraProfile (719)
CameraProfileDigest (12)                 CatEyeAmount (13)                        CatEyeScale (13)
Clarity (2)                              Clarity2012 (330)                        Cluster (959)
ColorGradeBlending (272)                 ColorGradeGlobalHue (272)                ColorGradeGlobalLum (272)
ColorGradeGlobalSat (272)                ColorGradeHighlightLum (272)             ColorGradeMidtoneHue (272)
ColorGradeMidtoneLum (272)               ColorGradeMidtoneSat (272)               ColorGradeShadowLum (272)
ColorNoiseReduction (68)                 ColorNoiseReductionDetail (61)           ColorNoiseReductionSmoothness (28)
CompatibleVersion (46)                   ContactInfo (959)                        Contrast (1)
Contrast2012 (70)                        ConvertToGrayscale (766)                 Copyright (959)
CorrectionActive (30)                    CorrectionAmount (30)                    CorrectionMasks (30)
CorrectionName (30)                      CorrectionSyncID (30)                    DefringeGreenAmount (1)
DefringeGreenHueHi (1)                   DefringeGreenHueLo (1)                   DefringePurpleAmount (1)
DefringePurpleHueHi (1)                  DefringePurpleHueLo (1)                  Dehaze (187)
Description (959)                        ErrorReason (30)                         Exposure (1)
Exposure2012 (44)                        FillLight (1)                            FocalRange (13)
GrainAmount (187)                        GrainFrequency (65)                      GrainSize (66)
GrayMixerAqua (77)                       GrayMixerBlue (94)                       GrayMixerGreen (94)
GrayMixerMagenta (94)                    GrayMixerOrange (94)                     GrayMixerPurple (94)
GrayMixerRed (94)                        GrayMixerYellow (94)                     GreenHue (131)
GreenSaturation (131)                    Group (959)                              HasSettings (959)
HighlightRecovery (1)                    Highlights2012 (50)                      HighlightsBoost (13)
HighlightsThreshold (13)                 HueAdjustmentAqua (275)                  HueAdjustmentBlue (275)
HueAdjustmentGreen (276)                 HueAdjustmentMagenta (274)               HueAdjustmentOrange (275)
HueAdjustmentPurple (274)                HueAdjustmentRed (276)                   HueAdjustmentYellow (276)
IncrementalTemperature (1)               IncrementalTint (1)                      ISO (4)
ISODependent (4)                         LensBlur (13)                            LensManualDistortionAmount (1)
LensProfileEnable (12)                   LensProfileSetup (11)                    LocalBlacks2012 (30)
LocalBrightness (30)                     LocalClarity (30)                        LocalClarity2012 (30)
LocalContrast (30)                       LocalContrast2012 (30)                   LocalCorrectedDepth (7)
LocalCurveRefineSaturation (7)           LocalDefringe (30)                       LocalDehaze (30)
LocalExposure (30)                       LocalExposure2012 (30)                   LocalGrain (7)
LocalHighlights2012 (30)                 LocalHue (30)                            LocalLuminanceNoise (30)
LocalMoire (30)                          LocalSaturation (30)                     LocalShadows2012 (30)
LocalSharpness (30)                      LocalTemperature (30)                    LocalTexture (30)
LocalTint (30)                           LocalToningHue (30)                      LocalToningSaturation (30)
LocalWhites2012 (30)                     Look (106)                               LookTable (60)
LuminanceAdjustmentAqua (275)            LuminanceAdjustmentBlue (275)            LuminanceAdjustmentGreen (276)
LuminanceAdjustmentMagenta (274)         LuminanceAdjustmentOrange (276)          LuminanceAdjustmentPurple (274)
LuminanceAdjustmentRed (275)             LuminanceAdjustmentYellow (272)          LuminanceNoiseReductionContrast (15)
LuminanceNoiseReductionDetail (15)       LuminanceSmoothing (68)                  MainCurve (1)
MaskActive (30)                          MaskBlendMode (30)                       MaskGroupBasedCorrections (30)
MaskInverted (30)                        MaskName (30)                            MaskSubCategoryID (12)
MaskSubType (30)                         MaskSyncID (30)                          MaskValue (30)
MaskVersion (30)                         Name (959)                               OverrideLookVignette (203)
ParametricDarks (276)                    ParametricHighlights (276)               ParametricHighlightSplit (277)
ParametricLights (276)                   ParametricMidtoneSplit (276)             ParametricShadows (276)
ParametricShadowSplit (276)              PerspectiveAspect (1)                    PerspectiveHorizontal (1)
PerspectiveRotate (1)                    PerspectiveScale (1)                     PerspectiveUpright (1)
PerspectiveVertical (1)                  PerspectiveX (1)                         PerspectiveY (1)
PointColors (1)                          PostCropVignetteAmount (189)             PostCropVignetteFeather (50)
PostCropVignetteHighlightContrast (30)   PostCropVignetteMidpoint (50)            PostCropVignetteRoundness (49)
PostCropVignetteStyle (37)               PresetType (959)                         ProcessVersion (924)
ProfileGainTableMap (10)                 ProfileToneCurve (2)                     RedHue (131)
RedSaturation (131)                      ReferencePoint (30)                      RequiresRGBTables (342)
RGBTable (492)                           RGBTableAmount (23)                      RGBTables (6)
Saturation (262)                         SaturationAdjustmentAqua (272)           SaturationAdjustmentBlue (276)
SaturationAdjustmentGreen (276)          SaturationAdjustmentMagenta (275)        SaturationAdjustmentOrange (278)
SaturationAdjustmentPurple (274)         SaturationAdjustmentRed (275)            SaturationAdjustmentYellow (276)
Shadows (1)                              Shadows2012 (56)                         ShadowTint (131)
SharpenDetail (59)                       SharpenEdgeMasking (59)                  SharpenRadius (59)
Sharpness (59)                           ShortName (959)                          ShowInPresets (15)
ShowInQuickActions (15)                  SortName (959)                           SphericalAberration (13)
SplitToningBalance (341)                 SplitToningHighlightHue (342)            SplitToningHighlightSaturation (342)
SplitToningShadowHue (342)               SplitToningShadowSaturation (342)        Stubbed (54)
SupportsAmount (959)                     SupportsAmount2 (143)                    SupportsColor (959)
SupportsHighDynamicRange (959)           SupportsMonochrome (959)                 SupportsNormalDynamicRange (959)
SupportsOutputReferred (959)             SupportsSceneReferred (959)              Temperature (10)
Texture (234)                            Tint (10)                                ToneCurve (6)
ToneCurveBlue (6)                        ToneCurveGreen (6)                       ToneCurveName (6)
ToneCurveName2012 (334)                  ToneCurvePV2012 (362)                    ToneCurvePV2012Blue (359)
ToneCurvePV2012Green (359)               ToneCurvePV2012Red (359)                 ToneCurveRed (6)
UUID (959)                               Version (959)                            Vibrance (265)
VignetteAmount (1)                       What (30)                                WhiteBalance (43)
Whites2012 (312)
```
