// ZZTDeathstar.c

/*
 
 Copyright (c) 2014, Paul Whitcomb
 All rights reserved.
 
 Redistribution and use in source and binary forms, with or without modification, are permitted provided that the following conditions are met:
 
 * Redistributions of source code must retain the above copyright notice, this list of conditions and the following disclaimer.
 * Redistributions in binary form must reproduce the above copyright notice, this list of conditions and the following disclaimer in the documentation and/or other materials provided with the distribution.
 * Neither the name of Paul Whitcomb nor the names of its contributors may be used to endorse or promote products derived from this software without specific prior written permission.
 
 THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS "AS IS" AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT OWNER OR CONTRIBUTORS
 BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT,
 STRICT LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 
 */

#include <stdio.h>
#include <time.h>
#include "ZZTDeathstar.h"
#include "ZZTTagData.h"

MapData openMapFromBuffer(void *buffer,uint32_t length) {
    MapData mapData;
    HaloMapHeader *mapHeader = ( HaloMapHeader *)(buffer);
    if(mapHeader->integrityHead == *(uint32_t *)&"deah" || mapHeader->integrityFoot == *(uint32_t *)&"toof") {
        mapData.error = MAP_INVALID_HEADER;
    }
    else if(mapHeader->indexOffset > length) {
        mapData.error = MAP_INVALID_INDEX_POINTER;
    }
    else {
        mapData.error = MAP_OK;
    }
    mapData.buffer = buffer;
    mapData.length = length;
    return mapData;
}

MapData openMapAtPath(const char *path) {
    FILE *map = fopen(path,"r");
    if(map)
    {
        fseek(map,0x0,SEEK_END);
        uint32_t length = (uint32_t)ftell(map);
        fseek(map,0x0,SEEK_SET);
        void *buffer = malloc(length);
        fread(buffer,length,0x1,map);
        fclose(map);
        return openMapFromBuffer(buffer, length);
    }
    else {
        MapData invalidMap;
        invalidMap.error = MAP_INVALID_PATH;
        return invalidMap;
    }
}

#define META_MEMORY_OFFSET 0x40440000 //Halo CE and Halo PC ONLY

static void zteam_deprotectClass(TagID tagId, char class[4]);
static void zteam_deprotectObjectTag(TagID tagId); //bipd, vehi, weap, eqip, garb, proj, scen, mach, ctrl, lifi, plac, obje, ssce
static void zteam_deprotectColl(TagID tagId);
static void zteam_deprotectEffe(TagID tagId);
static void zteam_deprotectFoot(TagID tagId);
static void zteam_deprotectItmc(TagID tagId);
static void zteam_deprotectMod2(TagID tagId);
static void zteam_deprotectPart(TagID tagId);
static void zteam_deprotectShdr(TagID tagId);
static void zteam_deprotectUnhi(TagID tagId);
static void zteam_deprotectGrhi(TagID tagId);
static void zteam_deprotectFont(TagID tagId);
static void zteam_deprotectHudDigits(TagID tagId);
static void zteam_deprotectHudg(TagID tagId);
static void zteam_deprotectSky(TagID tagId);
static void zteam_deprotectDeca(TagID tagId);
static void zteam_deprotectWphi(TagID tagId);
static void zteam_deprotectAntr(TagID tagId);

typedef enum {
    false = 0,
    true = 1
} bool;

bool haloCEmap = false;

typedef enum {
    OBJECT_BIPD = 0x0,
    OBJECT_VEHI = 0x1,
    OBJECT_WEAP = 0x2,
    OBJECT_EQIP = 0x3,
    OBJECT_GARB = 0x4,
    OBJECT_PROJ = 0x5,
    OBJECT_SCEN = 0x6,
    OBJECT_MACH = 0x7,
    OBJECT_CTRL = 0x8,
    OBJECT_LIFI = 0x9,
    OBJECT_PLAC = 0xA,
    OBJECT_SSCE = 0xB,
} TagObjectType;

typedef enum {
    SHADER_UNKN0 = 0x0, //unknown
    SHADER_UNKN1 = 0x1, //unknown
    SHADER_UNKN2 = 0x2, //unknown
    SHADER_SENV = 0x3,
    SHADER_SOSO = 0x4,
    SHADER_SOTR = 0x5,
    SHADER_SCHI = 0x6,
    SHADER_SCEX = 0x7,
    SHADER_SWAT = 0x8,
    SHADER_SGLA = 0x9,
    SHADER_SMET = 0xA,
    SHADER_SPLA = 0xB
} TagShaderType; //0x24

bool *deprotectedTags; //check for CERTAIN tags

MapTag *tagArray;
uint32_t tagCount;

uint32_t magic;

char *mapdata;
uint32_t mapdataSize;
uint32_t tagdataSize;

int saveMap(const char *path, MapData map) {
    FILE *mapFile = fopen(path,"wb");
    if(mapFile) {
        fwrite(map.buffer,1,map.length,mapFile);
        fclose(mapFile);
        return 0;
    }
    return 1;
}

static bool isNulledOut(TagID tag) {
    return (tag.tableIndex == 0 && tag.tagTableIndex == 0) || tag.tagTableIndex > tagCount;
}

static void *translatePointer(uint32_t pointer) { //translates a map pointer to where it points to in Deathstar
    return mapdata + (pointer - magic);
}

static void zteam_changeTagClass(TagID tagId,const char *class) {
    if(isNulledOut(tagId)) return;
    if(deprotectedTags[tagId.tagTableIndex]) return;
    tagArray[tagId.tagTableIndex].classA = *(uint32_t *)(class);
}

static inline void zteam_deprotectMultitextureOverlay(TagReflexive reflexive) {
    MultitextureOverlay *overlay = (MultitextureOverlay *)translatePointer(reflexive.offset);
    for(uint32_t i=0;i<reflexive.count;i++) {
        zteam_changeTagClass(overlay[i].mapPrimary.tagId, BITM);
        zteam_changeTagClass(overlay[i].mapSecondary.tagId, BITM);
        zteam_changeTagClass(overlay[i].mapTertiary.tagId,BITM);
    }
}

static void zteam_deprotectColl(TagID tagId) {
    if(isNulledOut(tagId)) return;
    if(deprotectedTags[tagId.tagTableIndex]) return;
    zteam_changeTagClass(tagId, COLL);
    deprotectedTags[tagId.tagTableIndex] = true;
    CollDependencies coll = *(CollDependencies *)translatePointer(tagArray[tagId.tagTableIndex].dataOffset);
    zteam_deprotectClass(coll.areaDamageEffect.tagId, coll.areaDamageEffect.mainClass);
    zteam_deprotectClass(coll.bodyDamagedEffect.tagId, coll.bodyDamagedEffect.mainClass);
    zteam_deprotectClass(coll.bodyDepletedEffect.tagId,coll.bodyDepletedEffect.mainClass);
    zteam_deprotectClass(coll.bodyDestroyedEffect.tagId, coll.bodyDestroyedEffect.mainClass);
    zteam_deprotectClass(coll.localizedDamageEffect.tagId, coll.localizedDamageEffect.mainClass);
    zteam_deprotectClass(coll.shieldDamagedEffect.tagId, coll.shieldDamagedEffect.mainClass);
    zteam_deprotectClass(coll.shieldDepletedEffect.tagId, coll.shieldDepletedEffect.mainClass);
    zteam_deprotectClass(coll.shieldRechargingEffect.tagId, coll.shieldRechargingEffect.mainClass);
    CollRegionsDependencies *regions = translatePointer(coll.regions.offset);
    for(uint32_t i=0;i<coll.regions.count;i++) {
        zteam_deprotectClass(regions[i].destroyedEffect.tagId, regions[i].destroyedEffect.mainClass);
    }
}

static void zteam_deprotectShdr(TagID tagId) {
    if(isNulledOut(tagId)) return; //however, this also means the map is broken
    if(deprotectedTags[tagId.tagTableIndex]) return;
    uint32_t tagClasses[] = {
        *(uint32_t *)&SHDR,
        *(uint32_t *)&SHDR,
        *(uint32_t *)&SHDR,
        *(uint32_t *)&SENV,
        *(uint32_t *)&SOSO,
        *(uint32_t *)&SOTR,
        *(uint32_t *)&SCHI,
        *(uint32_t *)&SCEX,
        *(uint32_t *)&SWAT,
        *(uint32_t *)&SGLA,
        *(uint32_t *)&SMET,
        *(uint32_t *)&SPLA
    };
    void *location = translatePointer(tagArray[tagId.tagTableIndex].dataOffset);
    Shader shdr = *(Shader *)location;
    if(shdr.type < 0x3 || shdr.type > sizeof(tagClasses) / 0x4) return;
    zteam_changeTagClass(tagId, (const char *)&tagClasses[shdr.type]);
    if(shdr.type == SHADER_SCEX) {
        ShaderScexDependencies scex = *(ShaderScexDependencies *)location;
        ShaderShaderLayersDependencies *layers = translatePointer(scex.layers.offset);
        for(uint32_t i=0; i<scex.layers.count; i++) {
            zteam_deprotectShdr(layers[i].shader.tagId);
        }
        zteam_changeTagClass(scex.lensflare.tagId, LENS);
        ShaderSchiMapDependencies *stage4maps = (ShaderSchiMapDependencies *)translatePointer(scex.stage4maps.offset);
        for(uint32_t i=0; i<scex.stage4maps.count;i++) {
            zteam_changeTagClass(stage4maps[i].map.tagId, BITM);
        }
        ShaderSchiMapDependencies *stage2maps = (ShaderSchiMapDependencies *)translatePointer(scex.stage2maps.offset);
        for(uint32_t i=0; i<scex.stage2maps.count;i++) {
            zteam_changeTagClass(stage2maps[i].map.tagId, BITM);
        }
    }
    else if(shdr.type == SHADER_SCHI) {
        ShaderSchiDependencies schi = *(ShaderSchiDependencies *)location;
        ShaderShaderLayersDependencies *layers = translatePointer(schi.layers.offset);
        for(uint32_t i=0; i<schi.layers.count; i++) {
            zteam_deprotectShdr(layers[i].shader.tagId);
        }
        ShaderSchiMapDependencies *maps = translatePointer(schi.maps.offset);
        for(uint32_t i=0; i<schi.maps.count; i++) {
            zteam_changeTagClass(maps[i].map.tagId, BITM);
        }
        zteam_changeTagClass(schi.lensflare.tagId, LENS);
    }
    else if(shdr.type == SHADER_SENV) {
        ShaderSenvDependencies senv = *(ShaderSenvDependencies *)location;
        zteam_changeTagClass(senv.baseMap.tagId, BITM);
        zteam_changeTagClass(senv.bumpMap.tagId, BITM);
        zteam_changeTagClass(senv.illuminationMap.tagId, BITM);
        zteam_changeTagClass(senv.lensFlare.tagId, LENS);
        zteam_changeTagClass(senv.microDetailMap.tagId, BITM);
        zteam_changeTagClass(senv.primaryDetailMap.tagId, BITM);
        zteam_changeTagClass(senv.secondaryDetailMap.tagId, BITM);
        zteam_changeTagClass(senv.reflectionCubeMap.tagId, BITM);
    }
    else if(shdr.type == SHADER_SGLA) {
        ShaderSglaDependencies sgla = *(ShaderSglaDependencies *)location;
        zteam_changeTagClass(sgla.bgTint.tagId, BITM);
        zteam_changeTagClass(sgla.bumpMap.tagId, BITM);
        zteam_changeTagClass(sgla.diffuseDetailMap.tagId, BITM);
        zteam_changeTagClass(sgla.diffuseMap.tagId, BITM);
        zteam_changeTagClass(sgla.reflectionMap.tagId, BITM);
        zteam_changeTagClass(sgla.specularDetailMap.tagId, BITM);
        zteam_changeTagClass(sgla.specularMap.tagId, BITM);
    }
    else if(shdr.type == SHADER_SMET) {
        ShaderSmetDependencies smet = *(ShaderSmetDependencies *)location;
        zteam_changeTagClass(smet.map.tagId, BITM);
    }
    else if(shdr.type == SHADER_SOSO) {
        ShaderSosoDependencies soso = *(ShaderSosoDependencies *)location;
        zteam_changeTagClass(soso.baseMap.tagId, BITM);
        zteam_changeTagClass(soso.detailMap.tagId, BITM);
        zteam_changeTagClass(soso.multiMap.tagId, BITM);
        zteam_changeTagClass(soso.reflectMap.tagId, BITM);
    }
    else if(shdr.type == SHADER_SOTR) {
        ShaderSotrDependencies sotr = *(ShaderSotrDependencies *)location;
        ShaderShaderLayersDependencies *layers = translatePointer(sotr.layers.offset);
        for(uint32_t i=0; i<sotr.layers.count; i++) {
            zteam_deprotectShdr(layers[i].shader.tagId);
        }
        ShaderSotrMapDependencies *maps = translatePointer(sotr.maps.offset);
        for(uint32_t i=0; i<sotr.maps.count; i++) {
            zteam_changeTagClass(maps[i].map.tagId, BITM);
        }
        zteam_changeTagClass(sotr.lensflare.tagId, LENS);
    }
    else if(shdr.type == SHADER_SPLA) {
        ShaderSplaDependencies spla = *(ShaderSplaDependencies *)location;
        zteam_changeTagClass(spla.primaryNoiseMap.tagId, BITM);
        zteam_changeTagClass(spla.secondaryNoiseMap.tagId, BITM);
    }
    else if(shdr.type == SHADER_SWAT) {
        ShaderSwatDependencies swat = *(ShaderSwatDependencies *)location;
        zteam_changeTagClass(swat.baseMap.tagId, BITM);
        zteam_changeTagClass(swat.reflectionMap.tagId, BITM);
        zteam_changeTagClass(swat.rippleMap.tagId, BITM);
    }
}

static void zteam_deprotectMod2(TagID tagId) {
    if(isNulledOut(tagId)) return;
    if(deprotectedTags[tagId.tagTableIndex]) return;
    zteam_changeTagClass(tagId,MOD2);
    Mod2Dependencies model = *( Mod2Dependencies *)translatePointer(tagArray[tagId.tagTableIndex].dataOffset);
    Mod2ShaderDependencies *shaders = translatePointer(model.mod2Shaders.offset);
    for(uint32_t i=0;i<model.mod2Shaders.count;i++) {
        zteam_deprotectShdr(shaders[i].shader.tagId);
    }
}

static void zteam_deprotectPart(TagID tagId) {
    if(isNulledOut(tagId)) return;
    if(deprotectedTags[tagId.tagTableIndex]) return;
    zteam_changeTagClass(tagId,PART);
    deprotectedTags[tagId.tagTableIndex] = true;
    PartDependencies part = *(PartDependencies *)translatePointer(tagArray[tagId.tagTableIndex].dataOffset);
    zteam_changeTagClass(part.bitmap.tagId, BITM);
    zteam_changeTagClass(part.physics.tagId, PPHY);
    zteam_changeTagClass(part.secondaryBitmap.tagId, BITM);
    zteam_deprotectClass(part.collisionEffect.tagId,part.collisionEffect.mainClass);
    zteam_deprotectClass(part.deathEffect.tagId,part.deathEffect.mainClass);
    zteam_deprotectFoot(part.materialEffects.tagId);
}

static void zteam_deprotectEffe(TagID tagId) {
    if(isNulledOut(tagId)) return;
    if(deprotectedTags[tagId.tagTableIndex]) return;
    zteam_changeTagClass(tagId, EFFE);
    deprotectedTags[tagId.tagTableIndex] = true;
    EffeDependencies *effe = (EffeDependencies *)translatePointer(tagArray[tagId.tagTableIndex].dataOffset);
    EffeEvents *events = translatePointer(effe->events.offset);
    for(uint32_t event=0;event<effe->events.count;event++) {
        EffeEventPartsDependencies *parts = translatePointer(events[event].parts.offset);
        for(uint32_t part=0;part < events[event].parts.count; part++) {
            zteam_deprotectClass(parts[part].type.tagId, parts[part].tagClass);
        }
        EffeEventParticlesDependencies *particles = translatePointer(events[event].particles.offset);
        for(uint32_t particle=0;particle<events[event].particles.count;particle++) {
            zteam_deprotectPart(particles[particle].particle.tagId);
        }
    }
}

static void zteam_deprotectFoot(TagID tagId) {
    if(isNulledOut(tagId)) return;
    if(deprotectedTags[tagId.tagTableIndex]) return;
    zteam_changeTagClass(tagId, FOOT);
    deprotectedTags[tagId.tagTableIndex] = true;
    FootDependencies *foot = translatePointer(tagArray[tagId.tagTableIndex].dataOffset);
    FootEffects *effects = translatePointer(foot->effects.offset);
    for(uint32_t effect = 0;effect < foot->effects.count; effect++)
    {
        FootEffectsMaterials *materials = translatePointer(effects[effect].materials.offset);
        for(uint32_t material = 0; material < effects[effect].materials.count;material++) {
            zteam_deprotectClass(materials[material].effect.tagId,materials[material].effect.mainClass);
            zteam_changeTagClass(materials[material].sound.tagId, SND);
        }
    }
}

static void zteam_deprotectFont(TagID tagId) {
    if(isNulledOut(tagId)) return;
    if(deprotectedTags[tagId.tagTableIndex]) return;
    zteam_changeTagClass(tagId, FONT);
    deprotectedTags[tagId.tagTableIndex] = true;
    FontDependencies font = *(FontDependencies *)translatePointer(tagArray[tagId.tagTableIndex].dataOffset);
    zteam_deprotectFont(font.boldFont.tagId);
    zteam_deprotectFont(font.italicFont.tagId);
    zteam_deprotectFont(font.condenseFont.tagId);
    zteam_deprotectFont(font.underlineFont.tagId);
}

static void zteam_deprotectDeca(TagID tagId) {
    if(isNulledOut(tagId)) return;
    if(deprotectedTags[tagId.tagTableIndex]) return;
    zteam_changeTagClass(tagId, DECA);
    deprotectedTags[tagId.tagTableIndex] = true;
    DecaDependencies deca = *(DecaDependencies *)translatePointer(tagArray[tagId.tagTableIndex].dataOffset);
    zteam_deprotectDeca(deca.nextDecal.tagId);
    zteam_changeTagClass(deca.shaderMap.tagId, BITM);
}

static void zteam_deprotectAntr(TagID tagId) {
    if(isNulledOut(tagId)) return;
    if(deprotectedTags[tagId.tagTableIndex]) return;
    zteam_changeTagClass(tagId, ANTR);
    deprotectedTags[tagId.tagTableIndex] = true;
    AntrDependencies antr = *(AntrDependencies *)translatePointer(tagArray[tagId.tagTableIndex].dataOffset);
    AntrSoundsDependencies *antrSounds = translatePointer(antr.sounds.offset);
    for(uint32_t i=0;i<antr.sounds.count;i++) {
        zteam_changeTagClass(antrSounds[i].sound.tagId, SND);
    }
}

static inline void zteam_deprotectWphiOverlay(TagReflexive overlay) {
    WphiOverlayElements *wphiOverlay = translatePointer(overlay.offset);
    for(uint32_t i=0;i<overlay.count;i++) {
        zteam_changeTagClass(wphiOverlay[i].bitmap.tagId, BITM);
    }
}

static void zteam_deprotectWphi(TagID tagId) {
    if(isNulledOut(tagId)) return;
    if(deprotectedTags[tagId.tagTableIndex]) return;
    zteam_changeTagClass(tagId, WPHI);
    deprotectedTags[tagId.tagTableIndex] = true;
    WphiDependencies wphi = *(WphiDependencies *)translatePointer(tagArray[tagId.tagTableIndex].dataOffset);
    WphiMeterElements *wphiME = translatePointer(wphi.meterElements.offset);
    for(uint32_t i=0;i<wphi.meterElements.count;i++) {
        zteam_changeTagClass(wphiME[i].bitmap.tagId, BITM);
    }
    
    WphiStaticElements *wphiSE = translatePointer(wphi.staticElements.offset);
    for(uint32_t i=0;i<wphi.staticElements.count;i++) {
        zteam_changeTagClass(wphiSE[i].bitmap.tagId, BITM);
        zteam_deprotectMultitextureOverlay(wphiSE[i].multitextureOverlay);
    }
    zteam_deprotectWphi(wphi.childHud.tagId);
    zteam_deprotectWphiOverlay(wphi.overlayElements);
    zteam_deprotectWphiOverlay(wphi.crosshairs);
    
    WphiScreenEffects *screenEffects = translatePointer(wphi.screenEffect.offset);
    for(uint32_t i=0;i<wphi.screenEffect.count;i++) {
        zteam_changeTagClass(screenEffects[i].maskFullscreen.tagId, BITM);
        zteam_changeTagClass(screenEffects[i].maskSplitscreen.tagId, BITM);
    }
    
}

static void zteam_deprotectUnhi(TagID tagId) {
    if(isNulledOut(tagId)) return;
    if(deprotectedTags[tagId.tagTableIndex]) return;
    zteam_changeTagClass(tagId, UNHI);
    deprotectedTags[tagId.tagTableIndex] = true;
    UnhiDependencies unhi = *(UnhiDependencies *)translatePointer(tagArray[tagId.tagTableIndex].dataOffset);
    zteam_deprotectMultitextureOverlay(unhi.auxOverlayMulitextureOverlay);
    zteam_deprotectMultitextureOverlay(unhi.healthBigMultitextureOverlay);
    zteam_deprotectMultitextureOverlay(unhi.hudBgMultitextureOverlay);
    zteam_deprotectMultitextureOverlay(unhi.motionSensorBgMultitextureOverlay);
    zteam_deprotectMultitextureOverlay(unhi.motionSensorFgMultitextureOverlay);
    zteam_deprotectMultitextureOverlay(unhi.shieldBgMultitextureOverlay);
    zteam_changeTagClass(unhi.healthInterfaceBitmap.tagId, BITM);
    zteam_changeTagClass(unhi.healthMeterBitmap.tagId, BITM);
    zteam_changeTagClass(unhi.hudinterfaceBitmap.tagId, BITM);
    zteam_changeTagClass(unhi.motionSensorBgInterfaceBitmap.tagId, BITM);
    zteam_changeTagClass(unhi.motionSensorFgInterfaceBitmap.tagId, BITM);
    zteam_changeTagClass(unhi.shieldInterfaceBitmap.tagId, BITM);
    zteam_changeTagClass(unhi.shieldMeterBitmap.tagId, BITM);
    UnhiHudMetersDependencies *meters = translatePointer(unhi.auxHudMeters.offset);
    for(uint32_t i=0;i<unhi.auxHudMeters.count;i++) {
        zteam_changeTagClass(meters[i].interfaceBitmap.tagId, BITM);
        zteam_changeTagClass(meters[i].meterBitmap.tagId, BITM);
    }
    UnhiHudWarningSoundsDependencies *sounds = translatePointer(unhi.hudWarningSounds.offset);
    for(uint32_t i=0;i<unhi.hudWarningSounds.count;i++) {
        zteam_changeTagClass(sounds[i].sound.tagId, sounds[i].sound.mainClass);
    }
}
static void zteam_deprotectObjectTag(TagID tagId) {
    if(isNulledOut(tagId)) return;
    if(deprotectedTags[tagId.tagTableIndex]) return;
    
    uint32_t tagClasses[] = {
        *(uint32_t *)&BIPD,
        *(uint32_t *)&VEHI,
        *(uint32_t *)&WEAP,
        *(uint32_t *)&EQIP,
        *(uint32_t *)&GARB,
        *(uint32_t *)&PROJ,
        *(uint32_t *)&SCEN,
        *(uint32_t *)&MACH,
        *(uint32_t *)&CTRL,
        *(uint32_t *)&LIFI,
        *(uint32_t *)&PLAC,
        *(uint32_t *)&SSCE
    };
    
    void *objectTag = translatePointer(tagArray[tagId.tagTableIndex].dataOffset);
    ObjeDependencies object = *( ObjeDependencies *)objectTag;
    
    if(object.tagObjectType > sizeof(tagClasses) / 0x4)
    {
        return; //failed to ID tag
    }
    
    zteam_changeTagClass(tagId,(const char *)&tagClasses[object.tagObjectType]);
    
    deprotectedTags[tagId.tagTableIndex] = true;
    
    zteam_deprotectMod2(object.model.tagId);
    zteam_deprotectAntr(object.animation.tagId);
    zteam_deprotectColl(object.collision.tagId);
    zteam_changeTagClass(object.physics.tagId,PHYS);
    zteam_deprotectShdr(object.shader.tagId);
    
    ObjeWidgets *widgets = (ObjeWidgets *)translatePointer(object.widgets.offset);
    for(uint32_t i=0;i<object.widgets.count;i++) {
        zteam_deprotectClass(widgets[i].name.tagId, widgets[i].name.mainClass);
    }
    
    ObjeAttachments *attachments = (ObjeAttachments *)translatePointer(object.attachments.offset);
    for(uint32_t i=0;i<object.attachments.count;i++) {
        zteam_deprotectClass(attachments[i].type.tagId, attachments[i].type.mainClass);
    }
    
    ObjeResources *resources = (ObjeResources *)translatePointer(object.resources.offset);
    for(uint32_t i=0;i<object.resources.count;i++) {
        if(resources[i].type == OBJE_TYPE_BITMAP) {
            zteam_changeTagClass(resources[i].name, BITM);
        }
        else if(resources[i].type == OBJE_TYPE_SOUND) {
            zteam_changeTagClass(resources[i].name, SND);
        }
    }
    
    if(object.tagObjectType == OBJECT_WEAP || object.tagObjectType == OBJECT_EQIP) {
        ItemDependencies item = *( ItemDependencies *)objectTag;
        zteam_deprotectFoot(item.materialEffects.tagId);
        zteam_changeTagClass(item.collisionSound.tagId, SND);
        zteam_deprotectClass(item.detonatingEffect.tagId,item.detonatingEffect.mainClass);
        zteam_deprotectClass(item.detonationEffect.tagId,item.detonationEffect.mainClass);
    }
    
    if(object.tagObjectType == OBJECT_WEAP) {
        WeapDependencies weap = *( WeapDependencies *)objectTag;
        zteam_deprotectMod2(weap.fpModel.tagId);
        zteam_deprotectAntr(weap.fpAnimation.tagId);
        WeapTriggerDependencies *triggers = translatePointer(weap.triggers.offset);
        for(uint32_t i=0;i<weap.triggers.count;i++) {
            zteam_deprotectObjectTag(triggers[i].projectile.tagId);
            zteam_deprotectClass(triggers[i].chargingEffect.tagId, triggers[i].chargingEffect.mainClass);
            WeapTriggerFiringEffects *firingEffects = translatePointer(triggers[i].firingEffect.offset);
            for(uint32_t fire=0;fire<triggers[i].firingEffect.count;fire++) {
                zteam_deprotectClass(firingEffects[fire].emptyEffect.tagId,firingEffects[fire].emptyEffect.mainClass);
                zteam_deprotectClass(firingEffects[fire].firingEffect.tagId,firingEffects[fire].firingEffect.mainClass);
                zteam_deprotectClass(firingEffects[fire].misfireEffect.tagId,firingEffects[fire].misfireEffect.mainClass);
                zteam_changeTagClass(firingEffects[fire].misfireDamage.tagId, JPT);
                zteam_changeTagClass(firingEffects[fire].emptyDamage.tagId, JPT);
                zteam_changeTagClass(firingEffects[fire].firingDamage.tagId, JPT);
            }
        }
        WeapMagazineDependencies *magazines = translatePointer(weap.magazines.offset);
        for(uint32_t i=0;i<weap.magazines.count;i++) {
            zteam_deprotectClass(magazines[i].chamberingEffect.tagId, magazines[i].chamberingEffect.mainClass);
            zteam_deprotectClass(magazines[i].reloadingEffect.tagId, magazines[i].reloadingEffect.mainClass);
            WeapMagazineMagazineDependencies *magequipment = translatePointer(magazines[i].weapMagazineEquipment.offset);
            for(uint32_t q=0;q<magazines[i].weapMagazineEquipment.count;q++) {
                zteam_deprotectObjectTag(magequipment[q].equipment.tagId);
            }
        }
        zteam_deprotectWphi(weap.hud.tagId);
        zteam_deprotectClass(weap.detonationEffect.tagId, weap.detonationEffect.mainClass);
        zteam_deprotectClass(weap.lightOffEffect.tagId, weap.lightOffEffect.mainClass);
        zteam_deprotectClass(weap.lightOnEffect.tagId, weap.lightOnEffect.mainClass);
        zteam_deprotectClass(weap.overheatedEffect.tagId, weap.overheatedEffect.mainClass);
        zteam_deprotectClass(weap.readyEffect.tagId, weap.readyEffect.mainClass);
        zteam_changeTagClass(weap.meleeDamage.tagId, JPT);
        zteam_changeTagClass(weap.meleeResponse.tagId, JPT);
        zteam_changeTagClass(weap.pickupSound.tagId,SND);
        zteam_changeTagClass(weap.zoomInSound.tagId,SND);
        zteam_changeTagClass(weap.zoomOutSound.tagId,SND);
    }
    
    if(object.tagObjectType == OBJECT_VEHI || object.tagObjectType == OBJECT_BIPD) {
        UnitDependencies unit = *( UnitDependencies *)objectTag;
        UnitWeaponDependencies *weapons = ( UnitWeaponDependencies *)translatePointer(unit.weapons.offset);
        for(uint32_t i=0;i<unit.weapons.count;i++) {
            zteam_deprotectObjectTag(weapons[i].weapon.tagId);
        }
        zteam_deprotectClass(unit.integratedLight.tagId,unit.integratedLight.mainClass);
        zteam_changeTagClass(unit.meleeDamage.tagId, JPT);
        zteam_changeTagClass(unit.spawnedActor.tagId, ACTV);
        UnitCameraTrackDependencies *trak = translatePointer(unit.cameraTrack.offset);
        for(uint32_t i=0;i<unit.cameraTrack.count;i++) {
            zteam_changeTagClass(trak[i].cameraTrack.tagId,TRAK);
        }
        UnitSeatsDependencies *seats = translatePointer(unit.seats.offset);
        for(uint32_t i=0;i<unit.seats.count;i++) {
            UnitSeatCameraTrackDependencies *unsc = translatePointer(seats[i].tracks.offset);
            for(uint32_t track = 0;track<seats[i].tracks.count;track++) {
                zteam_changeTagClass(unsc[track].cameraTrack.tagId, TRAK);
            }
            UnitSeatHudInterface *unhi = translatePointer(seats[i].unhi.offset);
            for(uint32_t umhi = 0;umhi < seats[i].unhi.count;umhi++) {
                zteam_deprotectUnhi(unhi[umhi].hud.tagId);
            }
        }
        //UnitDialogues *dialogues = translatePointer(unit.unitDialogue.offset);
        for(uint32_t i=0;i<unit.unitDialogue.count;i++) {
            
        }
        UnitNewHUDDependencies *unhis = translatePointer(unit.unitHud.offset);
        for(uint32_t i=0;i<unit.unitHud.count;i++) {
            zteam_deprotectUnhi(unhis[i].unhi.tagId);
        }
    }
    
    if(object.tagObjectType == OBJECT_VEHI) {
        VehiDependencies vehi = *(VehiDependencies *)objectTag;
        zteam_deprotectClass(vehi.effect.tagId,vehi.effect.mainClass);
        zteam_deprotectFoot(vehi.materialEffects.tagId);
        zteam_changeTagClass(vehi.crashSound.tagId, SND);
        zteam_changeTagClass(vehi.suspensionSound.tagId, SND);
    }
    
    if(object.tagObjectType == OBJECT_BIPD) {
        BipdDependencies bipd = *(BipdDependencies *)objectTag;
        zteam_deprotectFoot(bipd.materialEffects.tagId);
    }
    
    if(object.tagObjectType == OBJECT_PROJ) {
        ProjDependencies proj = *(ProjDependencies *)objectTag;
        zteam_deprotectClass(proj.superDetonation.tagId, proj.superDetonation.mainClass);
        zteam_deprotectClass(proj.superDetonation.tagId,proj.superDetonation.mainClass);
        zteam_changeTagClass(proj.attachedDamage.tagId, JPT);
        zteam_changeTagClass(proj.impactDamage.tagId, JPT);
        ProjMaterialResponseDependencies *respond = (ProjMaterialResponseDependencies *)translatePointer(proj.materialRespond.offset);
        for(uint32_t i=0;i<proj.materialRespond.count;i++) {
            zteam_deprotectClass(respond[i].defaultResult.tagId,respond[i].defaultResult.mainClass);
            zteam_deprotectClass(respond[i].detonationEffect.tagId,respond[i].detonationEffect.mainClass);
            zteam_deprotectClass(respond[i].potentialResult.tagId,respond[i].potentialResult.mainClass);
        }
    }
}

static void zteam_deprotectObjectPalette(TagReflexive reflexive) {
    ScnrPaletteDependency *palette = (ScnrPaletteDependency *)translatePointer(reflexive.offset);
    for(int i=0;i<reflexive.count;i++) {
        zteam_deprotectObjectTag(palette[i].object.tagId);
    }
}

static void zteam_deprotectMatgObjectTagCollection(TagReflexive reflexive) {
    MatgTagCollectionDependencies *collection = ( MatgTagCollectionDependencies *)translatePointer(reflexive.offset);
    for(int i=0;i<reflexive.count;i++) {
        zteam_deprotectObjectTag(collection[i].tag.tagId);
    }
}

static void zteam_deprotectItmc(TagID tagId) {
    if(isNulledOut(tagId))
        return;
    if(deprotectedTags[tagId.tagTableIndex]) return;
    zteam_changeTagClass(tagId,ITMC);
    deprotectedTags[tagId.tagTableIndex] = true;
    ItmcDependencies itmc = *( ItmcDependencies *)translatePointer(tagArray[tagId.tagTableIndex].dataOffset);
    ItmcPermutationDependencies *itmcPerm = ( ItmcPermutationDependencies *)translatePointer(itmc.permutation.offset);
    for(uint32_t i=0;i<itmc.permutation.count;i++) {
        zteam_deprotectObjectTag(itmcPerm[i].dependency.tagId);
    }
}

static void *translateCustomPointer(uint32_t pointer, uint32_t customMagic, uint32_t offset) {
    return mapdata + offset + (pointer - customMagic);
}

static void zteam_deprotectSBSP(TagID tagId,uint32_t fileOffset, uint32_t bspMagic) {
    if(isNulledOut(tagId))
        return;
    if(deprotectedTags[tagId.tagTableIndex]) return;
    zteam_changeTagClass(tagId, SBSP);
    deprotectedTags[tagId.tagTableIndex] = true;
    SBSPDependencies sbsp = *( SBSPDependencies *)(mapdata + fileOffset);
    SBSPCollisionMaterialsDependencies *materials = ( SBSPCollisionMaterialsDependencies *)translateCustomPointer(sbsp.collMaterials.offset,bspMagic,fileOffset);
    for(uint32_t i=0;i<sbsp.collMaterials.count;i++) {
        zteam_deprotectShdr(materials[i].shader.tagId);
    }
    SBSPLightmapsDependencies *lightmaps = ( SBSPLightmapsDependencies *)translateCustomPointer(sbsp.lightmaps.offset,bspMagic,fileOffset);
    for(uint32_t i=0;i<sbsp.lightmaps.count;i++) {
        SBSPLightmapsMaterialsReflexives *materials = ( SBSPLightmapsMaterialsReflexives *)translateCustomPointer(lightmaps[i].materials.offset, bspMagic, fileOffset);
        for(uint32_t q=0;q<lightmaps[i].materials.count;q++) {
            zteam_deprotectShdr(materials[q].shader.tagId);
        }
    }
}
static void zteam_deprotectHudg(TagID tagId) {
    if(isNulledOut(tagId))
        return;
    if(deprotectedTags[tagId.tagTableIndex]) return;
    zteam_changeTagClass(tagId, HUDG);
    deprotectedTags[tagId.tagTableIndex] = true;
    
    HudgDependencies hudg = *(HudgDependencies *)translatePointer(tagArray[tagId.tagTableIndex].dataOffset);
    zteam_changeTagClass(hudg.alternateIconText.tagId, USTR);
    zteam_changeTagClass(hudg.carnageReport.tagId, BITM);
    zteam_changeTagClass(hudg.checkpointSound.tagId,SND);
    zteam_changeTagClass(hudg.damageIndicatorBitmap.tagId, BITM);
    zteam_deprotectWphi(hudg.defaultWeaponHud.tagId);
    zteam_changeTagClass(hudg.hudMessages.tagId, HMT);
    zteam_changeTagClass(hudg.iconBitmap.tagId, BITM);
    zteam_changeTagClass(hudg.iconMessageText.tagId, USTR);
    zteam_deprotectFont(hudg.multiPlayerFont.tagId);
    zteam_deprotectFont(hudg.singlePlayerFont.tagId);
    zteam_changeTagClass(hudg.waypointArrowBitmap.tagId, BITM);
}

static void zteam_deprotectClass(TagID tagId, char class[4]) {
    if(isNulledOut(tagId))
        return;
    if(deprotectedTags[tagId.tagTableIndex]) return;
    uint32_t objectTagClasses[] = {
        *(uint32_t *)&BIPD,
        *(uint32_t *)&VEHI,
        *(uint32_t *)&WEAP,
        *(uint32_t *)&EQIP,
        *(uint32_t *)&GARB,
        *(uint32_t *)&PROJ,
        *(uint32_t *)&SCEN,
        *(uint32_t *)&MACH,
        *(uint32_t *)&CTRL,
        *(uint32_t *)&LIFI,
        *(uint32_t *)&PLAC,
        *(uint32_t *)&OBJE,
        *(uint32_t *)&SSCE
    };
    for(uint32_t i=0;i<sizeof(objectTagClasses)/4;i++) {
        if(*(uint32_t *)class == objectTagClasses[i]) {
            zteam_deprotectObjectTag(tagId);
            return;
        }
    }
    if(strncmp(class,EFFE,4) == 0) {
        zteam_deprotectEffe(tagId);
        return;
    }
    
    zteam_changeTagClass(tagId, class);
}

static void zteam_deprotectGrhi(TagID tagId) {
    if(isNulledOut(tagId))
        return;
    if(deprotectedTags[tagId.tagTableIndex]) return;
    zteam_changeTagClass(tagId, GRHI);
    deprotectedTags[tagId.tagTableIndex] = false;
    GrhiDependencies grhi = *(GrhiDependencies *)translatePointer(tagArray[tagId.tagTableIndex].dataOffset);
    zteam_changeTagClass(grhi.bgInterfaceBitmap.tagId, BITM);
    zteam_changeTagClass(grhi.interfaceBitmap.tagId, BITM);
    zteam_changeTagClass(grhi.overlayBitmap.tagId, BITM);
    zteam_deprotectMultitextureOverlay(grhi.bgMutlitextureOverlay);
    zteam_deprotectMultitextureOverlay(grhi.fgMultitextureOverlay);
}

static void zteam_deprotectHudDigits(TagID tagId) {
    if(isNulledOut(tagId))
        return;
    if(deprotectedTags[tagId.tagTableIndex]) return;
    zteam_changeTagClass(tagId, HUD);
    HudDependencies hud = *(HudDependencies *)translatePointer(tagArray[tagId.tagTableIndex].dataOffset);
    zteam_changeTagClass(hud.digitsBitmap.tagId,BITM);
}

static bool classCanBeDeprotected(uint32_t class) {
    
    uint32_t tagClasses[] = { //these tags should never ever be touched.
        *(uint32_t *)&DEVC,
        *(uint32_t *)&MATG,
        *(uint32_t *)&DELA,
        *(uint32_t *)&SOUL,
        *(uint32_t *)&TAGC,
        *(uint32_t *)&USTR
    };
    for(int i=0;i<sizeof(tagClasses)/4;i++) {
        if(class == tagClasses[i]) return false;
    }
    return true;
}

static bool classAutogeneric(uint32_t class) {
    uint32_t tagClasses[] = { //do not bother to deprotect these tags
        *(uint32_t *)&BITM,
        *(uint32_t *)&HUDG,
        *(uint32_t *)&SND,
        *(uint32_t *)&SBSP,
        *(uint32_t *)&SCNR,
        *(uint32_t *)&ITMC,
        *(uint32_t *)&FONT
    };
    for(int i=0;i<sizeof(tagClasses)/4;i++) {
        if(class == tagClasses[i]) return true;
    }
    return false;
}

static void zteam_deprotectSky(TagID tagId) {
    if(isNulledOut(tagId))
        return;
    if(deprotectedTags[tagId.tagTableIndex]) return;
    zteam_changeTagClass(tagId, SKY);
    SkyDependencies sky = *(SkyDependencies *)translatePointer(tagArray[tagId.tagTableIndex].dataOffset);
    zteam_deprotectMod2(sky.model.tagId);
    zteam_deprotectAntr(sky.animation.tagId);
    zteam_changeTagClass(sky.fog.tagId, FOG);
    SkyLensFlares *lensFlares = (SkyLensFlares *)translatePointer(sky.lensFlares.offset);
    for(uint32_t i=0;i<sky.lensFlares.count;i++) {
        zteam_deprotectClass(lensFlares[i].lensFlare.tagId,lensFlares[i].lensFlare.mainClass);
    }
}

#define MATCHING_THRESHOLD 0.7
#define MAX_TAG_NAME_SIZE 0x20

MapData name_deprotect(MapData map, MapData *maps, int map_count) {
    uint32_t length = map.length;
    
    HaloMapHeader *headerOldMap = ( HaloMapHeader *)(map.buffer);
    HaloMapIndex *indexOldMap = (HaloMapIndex *)(map.buffer + headerOldMap->indexOffset);
    tagCount = indexOldMap->tagCount;
    
    char *modded_buffer = calloc(map.length + MAX_TAG_NAME_SIZE * tagCount,0x1);
    
    memcpy(modded_buffer,map.buffer,length);
    
    HaloMapHeader *header = ( HaloMapHeader *)(modded_buffer);
    HaloMapIndex *index = ( HaloMapIndex *)(modded_buffer + header->indexOffset);
    
    haloCEmap = header->version == 609;
    
    mapdata = modded_buffer;
    magic = META_MEMORY_OFFSET - header->indexOffset;
    
    char *names = modded_buffer + length;
    int namesLength = 0x0;
    
    tagArray = ( MapTag *)(translatePointer(index->tagIndexOffset));
    tagCount = index->tagCount;
    
    for(uint32_t i=0;i<tagCount;i++) {
        if(!classCanBeDeprotected(tagArray[i].classA) || tagArray[i].nameOffset < META_MEMORY_OFFSET || tagArray[i].nameOffset > META_MEMORY_OFFSET + header->indexOffset) {
            continue;
        }
        
        if(haloCEmap && tagArray[i].notInsideMap)
            continue;
        if(strncmp(translatePointer(tagArray[i].nameOffset),"ui\\",3) == 0)
            continue;
        if(strncmp(translatePointer(tagArray[i].nameOffset),"sound\\",6) == 0)
            continue;
        
        const char *genericName = "deathstar\\%s\\tag";
        const char *tagClassName = translateHaloClassToName(tagArray[i].classA);
        char *bestTagTemp = malloc(strlen(tagClassName) + strlen(genericName) - 2);
        char *bestTag = bestTagTemp;
        sprintf(bestTag,genericName,tagClassName);
        
        if(!classAutogeneric(tagArray[i].classA)) {
            float bestMatch = MATCHING_THRESHOLD;
            for(int map=0;map<map_count;map++) {
                if(maps[map].error != MAP_OK) continue; //map is not a Halo cache map.
                float currentMatch = 0.0;
                //TODO: add fuzzy tag comparison
                if(currentMatch > bestMatch) {
                    //bestTag = currentTagName;
                }
            }
        }
        
        sprintf(names + namesLength, "%s_%u", bestTag, i); //all tags get a _# prefix
        
        free(bestTagTemp);
        
        int newname_length = (int)strlen(names + namesLength);
        
        tagArray[i].nameOffset = length + namesLength + magic;
        
        namesLength += newname_length + 0x1;
    }
    
    uint32_t new_length = length + namesLength;
    
    header->length = new_length;
    header->metaSize = new_length - header->indexOffset;
    
    MapData new_map;
    new_map.buffer = modded_buffer;
    new_map.length = new_length;
    new_map.error = MAP_OK;
    
    return new_map;
}

MapData zteam_deprotect(MapData map)
{
    MapData new_map;
    
    new_map.buffer = malloc(map.length);
    new_map.length = map.length;
    new_map.error = MAP_OK;
    
    memcpy(new_map.buffer,map.buffer,map.length);
    
    mapdata = new_map.buffer;
    uint32_t length = new_map.length;
    
    HaloMapHeader *header = ( HaloMapHeader *)(new_map.buffer);
    HaloMapIndex *index = ( HaloMapIndex *)(new_map.buffer + header->indexOffset);
    
    magic = META_MEMORY_OFFSET - header->indexOffset;
    
    tagArray = ( MapTag *)(translatePointer(index->tagIndexOffset));
    tagCount = index->tagCount;
    
    deprotectedTags = calloc(sizeof(bool) * tagCount,0x1);
    
    haloCEmap = header->version == 609;
    
    for(uint32_t i=0;i<tagCount;i++) {
        deprotectedTags[i] = haloCEmap && tagArray[i].notInsideMap;
    }
    
    mapdataSize = length;
    tagdataSize = length - header->indexOffset;
    
    TagID matgTag;
    matgTag.tableIndex = 0xFFFF;
    matgTag.tagTableIndex = 0xFFFF;
    
    for(int i=0;i<tagCount;i++) {
        char *name = translatePointer(tagArray[i].nameOffset);
        uint32_t class = tagArray[i].classA;
        if(class == *(uint32_t *)&MATG && strcmp(name,"globals\\globals") == 0) {
            matgTag = tagArray[i].identity;
            break;
        }
    }
    if(!isNulledOut(matgTag)) {
        deprotectedTags[matgTag.tagTableIndex] = true;
    }
    
    MapTag scenarioTag = tagArray[index->scenarioTag.tagTableIndex];
    zteam_changeTagClass(index->scenarioTag, SCNR);
    
    ScnrDependencies scnrData = *( ScnrDependencies *)translatePointer(scenarioTag.dataOffset);
    
    zteam_deprotectObjectPalette(scnrData.sceneryPalette);
    zteam_deprotectObjectPalette(scnrData.bipedPalette);
    zteam_deprotectObjectPalette(scnrData.equipPalette);
    zteam_deprotectObjectPalette(scnrData.vehiclePalette);
    zteam_deprotectObjectPalette(scnrData.weaponPalette);
    zteam_deprotectObjectPalette(scnrData.machinePalette);
    zteam_deprotectObjectPalette(scnrData.controlPalette);
    zteam_deprotectObjectPalette(scnrData.lifiPalette);
    zteam_deprotectObjectPalette(scnrData.sscePalette);
    
    ScnrStartingEquipment *equipment = (ScnrStartingEquipment *)translatePointer(scnrData.startingItmcs.offset);
    for(uint32_t i=0;i<scnrData.startingItmcs.count;i++) {
        for(int q=0;q<6;q++) {
            zteam_deprotectItmc(equipment[i].equipment[q].tagId);
        }
    }
    
    ScnrSkies *skies = ( ScnrSkies *)translatePointer(scnrData.skies.offset);
    for(uint32_t i=0;i<scnrData.skies.count;i++) {
        zteam_deprotectSky(skies[i].sky.tagId);
    }
    
    ScnrBSPs *bsps = ( ScnrBSPs *)translatePointer(scnrData.BSPs.offset);
    for(uint32_t i=0;i<scnrData.BSPs.count;i++) {
        zteam_deprotectSBSP(bsps[i].bsp.tagId, bsps[i].fileOffset, bsps[i].bspMagic);
    }
    
    Dependency *decas = (Dependency *)translatePointer(scnrData.decalPalette.offset);
    for(uint32_t i=0;i<scnrData.decalPalette.count;i++) {
        zteam_deprotectDeca(decas[i].tagId);
    }
    
    ScnrNetgameItmcDependencies *itmcs = ( ScnrNetgameItmcDependencies *)translatePointer(scnrData.netgameItmcs.offset);
    for(uint32_t i=0;i<scnrData.netgameItmcs.count;i++) {
        zteam_deprotectItmc(itmcs[i].itemCollection.tagId);
    }
    
    if(!isNulledOut(matgTag)) {
        MatgDependencies matg = *( MatgDependencies *)(translatePointer(tagArray[matgTag.tagTableIndex].dataOffset));
        zteam_deprotectMatgObjectTagCollection(matg.weapons);
        zteam_deprotectMatgObjectTagCollection(matg.powerups);
        
        MatgGrenadesDependencies *grenades = translatePointer(matg.grenades.offset);
        for(uint32_t i=0;i<matg.grenades.count;i++) {
            zteam_deprotectObjectTag(grenades[i].equipment.tagId);
            zteam_deprotectObjectTag(grenades[i].projectile.tagId);
            zteam_deprotectClass(grenades[i].throwingEffect.tagId, grenades[i].throwingEffect.mainClass);
            zteam_deprotectGrhi(grenades[i].hudInterface.tagId);
            
        }
        
        MatgInterfaceBitmapsDependencies *interfaceBitm = translatePointer(matg.interfaceBitm.offset);
        for(uint32_t i=0;i<matg.interfaceBitm.count;i++) {
            zteam_changeTagClass(interfaceBitm[i].dialogColorTable.tagId, COLO);
            zteam_changeTagClass(interfaceBitm[i].editorColorTable.tagId, COLO);
            zteam_deprotectFont(interfaceBitm[i].fontSystem.tagId);
            zteam_deprotectFont(interfaceBitm[i].fontTerminal.tagId);
            zteam_changeTagClass(interfaceBitm[i].hudColorTable.tagId, COLO);
            zteam_deprotectHudDigits(interfaceBitm[i].hudDigits.tagId);
            zteam_deprotectHudg(interfaceBitm[i].hudGlobals.tagId);
            zteam_changeTagClass(interfaceBitm[i].interfaceGoopMap1.tagId, BITM);
            zteam_changeTagClass(interfaceBitm[i].interfaceGoopMap2.tagId, BITM);
            zteam_changeTagClass(interfaceBitm[i].interfaceGoopMap3.tagId, BITM);
            zteam_changeTagClass(interfaceBitm[i].localization.tagId, STR);
            zteam_changeTagClass(interfaceBitm[i].motionSensorBlipBitmap.tagId, BITM);
            zteam_changeTagClass(interfaceBitm[i].motionSensorSweepBitmap.tagId, BITM);
            zteam_changeTagClass(interfaceBitm[i].motionSensorSweepBitmapMask.tagId, BITM);
            zteam_changeTagClass(interfaceBitm[i].multiplayerHudBitmap.tagId, BITM);
            zteam_changeTagClass(interfaceBitm[i].screenColorTable.tagId, COLO);
        }
        
        Dependency *cameraTracks = translatePointer(matg.camera.offset);
        for(uint32_t i=0;i<matg.camera.count;i++)
            zteam_deprotectClass(cameraTracks[i].tagId, TRAK);
        
        MatgPlayerInformationDependencies *playerInfo = translatePointer(matg.playerInfo.offset);
        for(uint32_t i=0;i<matg.playerInfo.count;i++) {
            zteam_deprotectObjectTag(playerInfo[i].unit.tagId);
        }
        
        MatgMultiplayerInformationDependencies *multiplayerInfo = translatePointer(matg.multiplayerInfo.offset);
        for(uint32_t i=0;i<matg.multiplayerInfo.count;i++) {
            zteam_deprotectObjectTag(multiplayerInfo[i].unit.tagId);
            zteam_deprotectObjectTag(multiplayerInfo[i].flag.tagId);
            zteam_deprotectObjectTag(multiplayerInfo[i].ball.tagId);
            zteam_deprotectMatgObjectTagCollection(multiplayerInfo[i].vehicles);
        }
    }
    
    free(deprotectedTags);
    
    return new_map;
}