/***************************************************************************
 *                                                                         *
 * Module  : D3D9texdic.c                                                    *
 *                                                                         *
 * Purpose :                                                               *
 *                                                                         *
 **************************************************************************/

/****************************************************************************
 Includes
 */

/* D3D9 Libraries */
#include <d3d9.h>

#include "batypes.h"
#include "batype.h"
#include "balibtyp.h"
#include "barwtyp.h"
#include "batextur.h"
#include "babinary.h"
#include "baimras.h"

#include "drvfns.h"
#include "d3d9device.h"
#include "d3d9raster.h"
#include "d3d9convrt.h"
#include "d3d9dxttex.h"

#include "d3d9texdic.h"

/****************************************************************************
 Defines
 */
#define HAS_ALPHA           (1<<0)
#define IS_CUBE             (1<<1)
#define USE_AUTOMIPMAPGEN   (1<<2)
#define IS_COMPRESSED       (1<<3)

/****************************************************************************
 Globals (across program)
 */

/****************************************************************************
 Local (static) Globals
 */

typedef struct _rwD3D9NativeTexture _rwD3D9NativeTexture;
struct _rwD3D9NativeTexture
{
    RwInt32 id; /* RwPlatformID,(rwID_D3D9) defined in batype.h */
    RwInt32 filterAndAddress; /* Same as in babintex.c */
    RwChar  name[rwTEXTUREBASENAMELENGTH]; /* Texture name */
    RwChar  mask[rwTEXTUREBASENAMELENGTH]; /* Texture mask name */
};

typedef struct _rwD3D9NativeRaster _rwD3D9NativeRaster;
struct _rwD3D9NativeRaster
{
    RwUInt32    format;         /* Raster format flags */
    D3DFORMAT   d3dFormat;      /* D3D pixel format */
    RwUInt16    width;          /* Raster width */
    RwUInt16    height;         /* Raster height */
    RwUInt8     depth;          /* Raster depth */
    RwUInt8     numMipLevels;   /* The number of mip levels to load */
    RwUInt8     type;           /* The raster type */
    RwUInt8     flags;          /* This raster has an alpha component, automipmapgen, etc */
};

/*
    rwCHUNKHEADERSIZE
        _rwD3D9NativeTexture
        _rwD3D9NativeRaster

        if ((format & rwRASTERFORMATPAL4) || (format & rwRASTERFORMATPAL8))
        {
            Load/Save palette
        }

        RwUInt32 - mip level size in bytes     --
        Data - mip level data                   | numMipLevels times
                    |                           |
                    --------------<--------------

 */

/****************************************************************************
 _rwD3D9NativeTextureGetSize

 On entry   :
 On exit    :
 */
RwBool
_rwD3D9NativeTextureGetSize(void *sizeIn, void *textureIn,
                          RwInt32 unused3 __RWUNUSED__)
{
    RwTexture       *texture = (RwTexture *)textureIn;
    RwRaster        *raster;
    _rwD3D9RasterExt  *rasExt;
    RwInt32         numMipLevels;
    RwUInt32        rasFormat;
    RwUInt32        size;

    RWFUNCTION(RWSTRING("_rwD3D9NativeTextureGetSize"));

    /* Platform specific ID, filter & addressing modes texture and Mask names */
    size = rwCHUNKHEADERSIZE + sizeof(_rwD3D9NativeTexture);

    raster = RwTextureGetRaster(texture);
    if (!raster)
    {
        *((RwUInt32 *)sizeIn) = size;

        RWRETURN(TRUE);
    }

    /* Native raster data structure */
    size += sizeof(_rwD3D9NativeRaster);

    /* Size of the palette if palletized */
    rasFormat = RwRasterGetFormat(raster);
    if (rasFormat & rwRASTERFORMATPAL4)
    {
        /* 32 is the smallest palette size that DX supports */
        size += sizeof(PALETTEENTRY) * 32;
    }
    else if (rasFormat & rwRASTERFORMATPAL8)
    {
        size += sizeof(PALETTEENTRY) * 256;
    }

    rasExt = RASTEREXTFROMRASTER(raster);

    /* Size of pixel data for all mip levels */
    if (rasExt->cube)
    {
        RwUInt32    cubeSize = 0;

        LPDIRECT3DCUBETEXTURE9 cubeTexture = (LPDIRECT3DCUBETEXTURE9)rasExt->texture;

        if (rasExt->automipmapgen)
        {
            numMipLevels = 1;
        }
        else
        {
            numMipLevels = IDirect3DCubeTexture9_GetLevelCount(cubeTexture);
        }

        while (numMipLevels--)
        {
            D3DSURFACE_DESC surfaceDesc;
            D3DLOCKED_RECT lockedRect;
            RwUInt32 mipmapsize;

            IDirect3DCubeTexture9_GetLevelDesc(cubeTexture, numMipLevels, &surfaceDesc);

            IDirect3DCubeTexture9_LockRect(cubeTexture, D3DCUBEMAP_FACE_POSITIVE_X, numMipLevels, &lockedRect, NULL, D3DLOCK_NO_DIRTY_UPDATE | D3DLOCK_READONLY);
            IDirect3DCubeTexture9_UnlockRect(cubeTexture, D3DCUBEMAP_FACE_POSITIVE_X, numMipLevels);

            if (rasExt->compressed)
            {
                if (surfaceDesc.Height < 4)
                {
                    mipmapsize = lockedRect.Pitch;
                }
                else
                {
                    mipmapsize = (surfaceDesc.Height / 4) * lockedRect.Pitch;
                }
            }
            else
            {
                mipmapsize = surfaceDesc.Height * lockedRect.Pitch;
            }

            cubeSize += sizeof(RwUInt32) + mipmapsize;
        }

        size += 6 * cubeSize;
    }
    else
    {

        if (rasExt->automipmapgen)
        {
            numMipLevels = 1;
        }
        else
        {
            numMipLevels = IDirect3DTexture9_GetLevelCount(rasExt->texture);
        }

        while (numMipLevels--)
        {
            D3DSURFACE_DESC surfaceDesc;
            D3DLOCKED_RECT lockedRect;
            RwUInt32 mipmapsize;

            IDirect3DTexture9_GetLevelDesc(rasExt->texture, numMipLevels, &surfaceDesc);

            IDirect3DTexture9_LockRect(rasExt->texture, numMipLevels, &lockedRect, NULL, D3DLOCK_NO_DIRTY_UPDATE | D3DLOCK_READONLY);
            IDirect3DTexture9_UnlockRect(rasExt->texture, numMipLevels);

            if (rasExt->compressed)
            {
                if (surfaceDesc.Height < 4)
                {
                    mipmapsize = lockedRect.Pitch;
                }
                else
                {
                    mipmapsize = (surfaceDesc.Height / 4) * lockedRect.Pitch;
                }
            }
            else
            {
                mipmapsize = surfaceDesc.Height * lockedRect.Pitch;
            }

            size += sizeof(RwUInt32) + mipmapsize;
        }
    }

    *((RwUInt32 *)sizeIn) = size;

    RWRETURN(TRUE);
}

/****************************************************************************
 _rwD3D9NativeTextureWrite

 On entry   :
 On exit    :
 */
RwBool
_rwD3D9NativeTextureWrite(void *streamIn, void *textureIn,
                        RwInt32 unused3 __RWUNUSED__)
{
    RwStream            *stream = (RwStream *)streamIn;
    RwTexture           *texture = (RwTexture *)textureIn;
    RwRaster            *raster;
    _rwD3D9RasterExt      *rasExt;
    _rwD3D9NativeTexture  nativeTexture;
    _rwD3D9NativeRaster   nativeRaster;
    RwInt32             bytesLeftToWrite;
    RwInt32             i, face;

    RWFUNCTION(RWSTRING("_rwD3D9NativeTextureWrite"));

    /* Calc the amount of data to write, excluding the chunk header*/
    _rwD3D9NativeTextureGetSize((void *)&bytesLeftToWrite, textureIn, 0);
    bytesLeftToWrite -= rwCHUNKHEADERSIZE;

    /* Struct header for _rwD3DNativeTexture structure */
    if (!RwStreamWriteChunkHeader(stream, rwID_STRUCT, bytesLeftToWrite))
    {
        RWRETURN(FALSE);
    }

    /*
     * Don't have to worry about endianness as this is platform specific
     */

    /* ID, filter & addressing modes */
    nativeTexture.id = rwID_PCD3D9;
    nativeTexture.filterAndAddress =
        (((RwInt32)RwTextureGetFilterMode(texture)) & 0xFF) |
        ((((RwInt32)RwTextureGetAddressingU(texture)) << 8) & 0x0F00) |
        ((((RwInt32)RwTextureGetAddressingV(texture)) << 12) & 0xF000);

    /* Texture name */
    memcpy(nativeTexture.name, RwTextureGetName(texture),
        sizeof(RwChar) * rwTEXTUREBASENAMELENGTH);

    /* Mask name */
    memcpy(nativeTexture.mask, RwTextureGetMaskName(texture),
        sizeof(RwChar) * rwTEXTUREBASENAMELENGTH);

    if (!RwStreamWrite(stream, (void *)&nativeTexture,
                       sizeof(_rwD3D9NativeTexture)))
    {
        RWRETURN(FALSE);
    }

    bytesLeftToWrite -= sizeof(_rwD3D9NativeTexture);

    /*
     * Write the rasters
     */
    raster = RwTextureGetRaster(texture);
    rasExt = RASTEREXTFROMRASTER(raster);

    nativeRaster.width = (RwUInt16)RwRasterGetWidth(raster);
    nativeRaster.height = (RwUInt16)RwRasterGetHeight(raster);
    nativeRaster.format = RwRasterGetFormat(raster);
    nativeRaster.depth = (RwUInt8)RwRasterGetDepth(raster);
    nativeRaster.numMipLevels = (RwUInt8)RwRasterGetNumLevels(raster);
    nativeRaster.type = (RwUInt8)raster->cType;

    nativeRaster.flags = 0;
    if (rasExt->alpha)
    {
        nativeRaster.flags |= HAS_ALPHA;
    }

    if (rasExt->cube)
    {
        nativeRaster.flags |= IS_CUBE;
    }

    if (rasExt->automipmapgen)
    {
        nativeRaster.flags |= USE_AUTOMIPMAPGEN;
    }

    if (rasExt->compressed)
    {
        nativeRaster.flags |= IS_COMPRESSED;
    }

    nativeRaster.d3dFormat = rasExt->d3dFormat;
    RWASSERT(nativeRaster.d3dFormat > 0);

    if (!RwStreamWrite(stream, &nativeRaster, sizeof(_rwD3D9NativeRaster)))
    {
        RWRETURN(FALSE);
    }

    bytesLeftToWrite -= sizeof(_rwD3D9NativeRaster);

    if (nativeRaster.format & rwRASTERFORMATPAL4)
    {
        RwUInt8 *palette;

        palette = RwRasterLockPalette(raster, rwRASTERLOCKREAD);

        /* 32 is the smallest palette size that DX supports */
        if (!RwStreamWrite(stream, (void *)palette, sizeof(PALETTEENTRY) * 32))
        {
            RWRETURN(FALSE);
        }

        RwRasterUnlockPalette(raster);

        bytesLeftToWrite -= sizeof(PALETTEENTRY) * 32;
    }
    else if (nativeRaster.format & rwRASTERFORMATPAL8)
    {
        RwUInt8 *palette;

        palette = RwRasterLockPalette(raster, rwRASTERLOCKREAD);

        /* 32 is the smallest palette size that DX supports */
        if (!RwStreamWrite(stream, (void *)palette, sizeof(PALETTEENTRY) * 256))
        {
            RWRETURN(FALSE);
        }

        RwRasterUnlockPalette(raster);

        bytesLeftToWrite -= sizeof(PALETTEENTRY) * 256;
    }

    if (rasExt->cube)
    {
        face = 6;
    }
    else
    {
        face = 1;
    }

    rasExt->face = 0;

    do
    {
        for (i = 0; i < nativeRaster.numMipLevels; i++)
        {
            D3DSURFACE_DESC surfaceDesc;
            RwUInt32        size;
            RwUInt8         *pixels;

            pixels = RwRasterLock(raster, (RwUInt8)i, rwRASTERLOCKREAD);
            RWASSERT(NULL != pixels);

            if (!pixels)
            {
                RWRETURN(FALSE);
            }

            if (rasExt->cube)
            {
                IDirect3DCubeTexture9_GetLevelDesc((LPDIRECT3DCUBETEXTURE9)rasExt->texture, i, &surfaceDesc);
            }
            else
            {
                IDirect3DTexture9_GetLevelDesc(rasExt->texture, i, &surfaceDesc);
            }

            if (rasExt->compressed)
            {
                if (surfaceDesc.Height < 4)
                {
                    size = rasExt->lockedRect.Pitch;
                }
                else
                {
                    size = (surfaceDesc.Height / 4) * (rasExt->lockedRect.Pitch);
                }
            }
            else
            {
                size = surfaceDesc.Height * (rasExt->lockedRect.Pitch);
            }

            if (!RwStreamWrite(stream, (void *)&size, sizeof(RwUInt32)))
            {
                RWRETURN(FALSE);
            }

            bytesLeftToWrite -= sizeof(RwUInt32);

            /*
            * Current implementation of fwrite seem to cough on large write
            */

            /* Don't delete these C++ comment John, unless the above is fixed */
            /*
            if (!RwStreamWrite(stream, (void *)pixels, size))
            {
                RWRETURN(FALSE);
            }
            */
            {
                RwInt32 numBlocks;

                numBlocks = size / 1024;

                while (numBlocks--)
                {
                    if (!RwStreamWrite(stream, (void *)pixels, 1024))
                    {
                        RWRETURN(FALSE);
                    }

                    pixels += 1024;
                    size -= 1024;
                    bytesLeftToWrite -= 1024;
                }

                if (0 != size)
                {
                    if (!RwStreamWrite(stream, (void *)pixels, size))
                    {
                        RWRETURN(FALSE);
                    }

                    bytesLeftToWrite -= size;
                }
            }

            RwRasterUnlock(raster);

            if (rasExt->automipmapgen)
            {
                break; /* Only write first mip level */
            }
        }

        if (rasExt->cube)
        {
            rasExt->face += 1;
        }
    }
    while(--face);

    RWASSERT(0 == bytesLeftToWrite);

    RWRETURN(TRUE);
}

/****************************************************************************
 _rwD3D9NativeTextureRead

 On entry   :
 On exit    :
 */
#ifdef RW_BRANCH_COMMDBG

#define MAX_PIXEL_FORMATS   128

static RwBool _rwD3D9NativeTextureIsRawD3DFMT( D3DFORMAT fmt, size_t *pixelbytesize_out )
{
    if ( fmt < 0u || fmt >= MAX_PIXEL_FORMATS )
    {
        return FALSE;
    }

    _rwD3D9FormatInfo *finfo = &_rwD3D9PixelFormatInfo[ fmt ];

    if ( finfo->depth == 0 )
    {
        return FALSE;
    }
    
    *pixelbytesize_out = ( finfo->depth / 8u );
    return TRUE;
}

struct _rwD3D9CompressionInfo
{
    RwUInt8 pixblock_w, pixblock_h;
    RwUInt8 blockbytesize;
};

static RwBool _rwD3D9NativeTextureIsCompressionFormat( D3DFORMAT fmt, struct _rwD3D9CompressionInfo *cinfo_out )
{
    if ( fmt == D3DFMT_DXT1 || 
         fmt == D3DFMT_DXT2 ||
         fmt == D3DFMT_DXT3 ||
         fmt == D3DFMT_DXT4 ||
         fmt == D3DFMT_DXT5 )
    {
        cinfo_out->pixblock_w = 4;
        cinfo_out->pixblock_h = 4;
        
        if ( fmt == D3DFMT_DXT1 )
        {
            cinfo_out->blockbytesize = 8;
        }
        else
        {
            cinfo_out->blockbytesize = 16;
        }
        return TRUE;
    }
    else if ( fmt == D3DFMT_UYVY ||
              fmt == D3DFMT_R8G8_B8G8 ||
              fmt == D3DFMT_YUY2 ||
              fmt == D3DFMT_G8R8_G8B8 )
    {
        cinfo_out->pixblock_w = 2;
        cinfo_out->pixblock_h = 1;
        cinfo_out->blockbytesize = 4;
        return TRUE;
    }

    return FALSE;
}

// Known unsupported formats:
// * D3DFMT_VERTEXDATA
// * D3DFMT_INDEX16
// * D3DFMT_INDEX32
// * D3DFMT_MULTI2_ARGB8

static size_t _rwD3D9NativeTextureGetVideoBufferSize( RwRaster *ras, _rwD3D9RasterExt *rasExt, RwUInt32 mipidx )
{
    // Get the raster type that is important for the video buffer dimensions.
    RwUInt8 rasType = ras->cType;

    RwUInt32 base_width, base_height;

    if ( rasType == rwRASTERTYPETEXTURE || rasType == rwRASTERTYPENORMAL ||
         rasType == rwRASTERTYPEZBUFFER || rasType == rwRASTERTYPECAMERATEXTURE )
    {
        // These raster types come with their own (base) texture handle.
        if ( rasExt->cube == TRUE )
        {
            base_width = (RwUInt32)ras->originalWidth;
            base_height = base_width;
        }
        else
        {
            base_width = (RwUInt32)ras->originalWidth;
            base_height = (RwUInt32)ras->originalHeight;
        }
    }
    else if ( rasType == rwRASTERTYPECAMERA )
    {
        // The camera does only have one mip layer, always.
        if ( mipidx > 0 )
        {
            return 0;
        }

        // Camera rasters do always target the back-buffer.
        base_width = Present.BackBufferWidth;
        base_height = Present.BackBufferHeight;
    }
    else
    {
        // Invalid raster type.
        return 0;
    }

    // Calculate the mip-level dimensions.
    RwUInt32 raw_mip_width = base_width;
    RwUInt32 raw_mip_height = base_height;

    for ( RwUInt32 n = 0; n < mipidx; n++ )
    {
        if ( raw_mip_width == 1 && raw_mip_height == 1 )
        {
            // No mipmap layer.
            return 0;
        }

        if ( raw_mip_width > 1 )
        {
            raw_mip_width /= 2;
        }

        if ( raw_mip_height > 1 )
        {
            raw_mip_height /= 2;
        }
    }

    // Calculate the video buffer size depending on the surface pixel format.
    // We do not support all the pixel formats thus we limit ourselves to the known subset.
    D3DFORMAT fmt = rasExt->d3dFormat;

    // Try raw format.
    {
        RwUInt32 samplebytesize;

        if ( _rwD3D9NativeTextureIsRawD3DFMT( fmt, &samplebytesize ) )
        {
            return ( ras->stride * raw_mip_height );
        }
    }

    // Try compressed format.
    {
        struct _rwD3D9CompressionInfo cinfo;

        if ( _rwD3D9NativeTextureIsCompressionFormat( fmt, &cinfo ) )
        {
            RwUInt32 blocks_by_height = ( ( raw_mip_height + ( cinfo.pixblock_h - 1 ) ) / cinfo.pixblock_h );

            return ( ras->stride * blocks_by_height );
        }
    }

    // We have no idea. Better not allow loading.
    return 0;
}

#endif //RW_BRANCH_COMMDBG


RwBool
_rwD3D9NativeTextureRead(void *streamIn, void *textureIn,
                       RwInt32 unused3 __RWUNUSED__)
{
    RwStream            *stream = (RwStream *)streamIn;
    RwRaster            *raster = (RwRaster *)NULL;
    RwTexture           *texture;
    RwUInt32            length, version;
    _rwD3D9NativeRaster   nativeRaster;
    _rwD3D9NativeTexture  nativeTexture;
    _rwD3D9RasterExt      *rasExt;

    RWFUNCTION(RWSTRING("_rwD3D9NativeTextureRead"));

    if (!RwStreamFindChunk(stream, rwID_STRUCT, &length, &version))
    {
        RWRETURN(FALSE);
    }

    RWASSERT(version >= rwLIBRARYBASEVERSION);
    RWASSERT(version <= rwLIBRARYCURRENTVERSION);
    if (!((version >= rwLIBRARYBASEVERSION) &&
         (version <= rwLIBRARYCURRENTVERSION)))
    {
        RWRETURN(FALSE);
    }

    /*
     * Don't have to worry about endianness as this is platform specific
     */

    /* ID, filter & addressing modes */
    if (RwStreamRead(stream, (void *)&nativeTexture,
        sizeof(_rwD3D9NativeTexture)) != sizeof(_rwD3D9NativeTexture))
    {
        RWRETURN(FALSE);
    }

    /* Check native texture ID */
    RWASSERT(rwID_PCD3D9 == nativeTexture.id);

    if (rwID_PCD3D9 != nativeTexture.id)
    {
        RWRETURN(FALSE);
    }

    /* Native raster data structure */
    if (RwStreamRead(stream, (void *)&nativeRaster,
        sizeof(_rwD3D9NativeRaster)) != sizeof(_rwD3D9NativeRaster))
    {
        RWRETURN(FALSE);
    }

    if (nativeRaster.flags & IS_COMPRESSED)
    {
        RwUInt32 dxtFormat;

        /* Get compressed format */
        dxtFormat = nativeRaster.d3dFormat;

        if (_rwD3D9CheckValidTextureFormat(dxtFormat))
        {
            RwUInt32 levels;

            /* Create a raster */
            raster = RwRasterCreate(nativeRaster.width,
                                    nativeRaster.height,
                                    (RwInt32)nativeRaster.depth,
                                    nativeRaster.type | nativeRaster.format |
                                    rwRASTERDONTALLOCATE);

            if (NULL == raster)
            {
                RWRETURN(FALSE);
            }

            /* Get the raster extension */
            rasExt = RASTEREXTFROMRASTER(raster);

            levels = ((RwRasterGetFormat(raster) & rwRASTERFORMATMIPMAP) ?
                      nativeRaster.numMipLevels : 1);

            if ((nativeRaster.flags & IS_CUBE) != 0)
            {
                if ((_RwD3D9DeviceCaps.CubeTextureFilterCaps & (D3DPTFILTERCAPS_MIPFPOINT | D3DPTFILTERCAPS_MIPFLINEAR)) == 0)
                {
                    if (levels > 1)
                    {
                        RwRasterDestroy(raster);

                        RwDebugSendMessage(rwDEBUGMESSAGE, "_rwD3D9NativeTextureRead",
                                          "Cube texture mip-mapping not supported");

                        RWRETURN(FALSE);
                    }
                }

                rasExt->cube = TRUE;

                /* check automipmapgen */
                if ((nativeRaster.format & (rwRASTERFORMATAUTOMIPMAP | rwRASTERFORMATMIPMAP)) == (rwRASTERFORMATAUTOMIPMAP | rwRASTERFORMATMIPMAP))
                {
                    if (_rwD3D9CheckAutoMipmapGenCubeTextureFormat(dxtFormat))
                    {
                        rasExt->automipmapgen = 1;
                    }
                }

                if (FAILED(IDirect3DDevice9_CreateCubeTexture(_RwD3DDevice,
                                                            raster->width,
                                                            levels,
                                                            (rasExt->automipmapgen ? D3DUSAGE_AUTOGENMIPMAP : 0),
                                                            (D3DFORMAT)dxtFormat,
                                                            D3DPOOL_MANAGED,
                                                            (LPDIRECT3DCUBETEXTURE9 *)&(rasExt->texture),
                                                            NULL)))
                {
                    RwRasterDestroy(raster);

                    RwDebugSendMessage(rwDEBUGMESSAGE, "_rwD3D9NativeTextureRead",
                                    "Compressed cube texture format not supported.");

                    RWRETURN(FALSE);
                }
                else
                {
                    /* Remove the rwRASTERDONTALLOCATE flag so it will get destroyed */
                    raster->cFlags &= ~rwRASTERDONTALLOCATE;

                    /* Copy alpha flag */
                    rasExt->alpha = (nativeRaster.flags & HAS_ALPHA) != 0;

                    /* Mark as cube texture */
                    rasExt->cube = TRUE;

                    /* Mark as compressed texture */
                    rasExt->compressed = TRUE;

                    rasExt->d3dFormat = dxtFormat;
                }
            }
            else
            {
                /* check automipmapgen */
                if ((nativeRaster.format & (rwRASTERFORMATAUTOMIPMAP | rwRASTERFORMATMIPMAP)) == (rwRASTERFORMATAUTOMIPMAP | rwRASTERFORMATMIPMAP))
                {
                    if (_rwD3D9CheckAutoMipmapGenTextureFormat(dxtFormat))
                    {
                        rasExt->automipmapgen = 1;
                    }
                }

                /* Create the texture, if mipped 0 gets all the mip levels */
                if (FAILED(IDirect3DDevice9_CreateTexture(_RwD3DDevice,
                                                        raster->width,
                                                        raster->height,
                                                        levels,
                                                        (rasExt->automipmapgen ? D3DUSAGE_AUTOGENMIPMAP : 0),
                                                        (D3DFORMAT)dxtFormat,
                                                        D3DPOOL_MANAGED,
                                                        &(rasExt->texture),
                                                        NULL)))
                {
                    RwRasterDestroy(raster);

                    RwDebugSendMessage(rwDEBUGMESSAGE, "_rwD3D9NativeTextureRead",
                                    "Compressed texture format not supported.");

                    RWRETURN(FALSE);
                }
                else
                {
                    /* Remove the rwRASTERDONTALLOCATE flag so it will get destroyed */
                    raster->cFlags &= ~rwRASTERDONTALLOCATE;

                    /* Copy alpha flag */
                    rasExt->alpha = (nativeRaster.flags & HAS_ALPHA) != 0;

                    /* Mark as compressed texture */
                    rasExt->compressed = TRUE;

                    rasExt->d3dFormat = dxtFormat;
                }
            }

            #if defined(RWDEBUG)
            NumTextureRasters++;
            #endif /* defined(RWDEBUG) */
        }
        else
        {
            RwDebugSendMessage(rwDEBUGMESSAGE, "_rwD3D9NativeTextureRead",
                               "Compressed texture format not supported.");

            RWRETURN(FALSE);
        }
    }
    else if ((nativeRaster.flags & IS_CUBE) != 0)
    {
        // THIS CODE PATH NEVER WORKS.

        RwUInt32 levels;

        /* Create a raster */
        raster = RwRasterCreate(nativeRaster.width,
                                nativeRaster.height,
                                (RwInt32)nativeRaster.depth,
                                nativeRaster.type | nativeRaster.format |
                                rwRASTERDONTALLOCATE);

        if (NULL == raster)
        {
            RWRETURN(FALSE);
        }

        /* Get the raster extension */
        rasExt = RASTEREXTFROMRASTER(raster);

        levels = ((RwRasterGetFormat(raster) & rwRASTERFORMATMIPMAP) ?
                  nativeRaster.numMipLevels : 1);

        if ((_RwD3D9DeviceCaps.CubeTextureFilterCaps & (D3DPTFILTERCAPS_MIPFPOINT | D3DPTFILTERCAPS_MIPFLINEAR)) == 0)
        {
            if (levels > 1)
            {
                RwRasterDestroy(raster);

                RwDebugSendMessage(rwDEBUGMESSAGE, "_rwD3D9NativeTextureRead",
                                    "Cube texture mip-mapping not supported");

                RWRETURN(FALSE);
            }
        }

        /* Mark as cube texture */
        rasExt->cube = TRUE;

        /* check automipmapgen */
        if ((nativeRaster.format & (rwRASTERFORMATAUTOMIPMAP | rwRASTERFORMATMIPMAP)) == (rwRASTERFORMATAUTOMIPMAP | rwRASTERFORMATMIPMAP))
        {
            if (_rwD3D9CheckAutoMipmapGenCubeTextureFormat(nativeRaster.d3dFormat))
            {
                rasExt->automipmapgen = 1;
            }
        }

        if (FAILED(IDirect3DDevice9_CreateCubeTexture(_RwD3DDevice,
                                                      raster->width,
                                                      levels,
                                                      (rasExt->automipmapgen ? D3DUSAGE_AUTOGENMIPMAP : 0),
                                                      nativeRaster.d3dFormat,
                                                      D3DPOOL_MANAGED,
                                                      (LPDIRECT3DCUBETEXTURE9 *)&(rasExt->texture),
                                                      NULL)))
        {
            RwRasterDestroy(raster);

            RwDebugSendMessage(rwDEBUGMESSAGE, "_rwD3D9NativeTextureRead",
                            "Compressed cube texture format not supported.");

            RWRETURN(FALSE);
        }
        else
        {
            /* Remove the rwRASTERDONTALLOCATE flag so it will get destroyed */
            raster->cFlags &= ~rwRASTERDONTALLOCATE;

            /* Copy alpha flag */
            rasExt->alpha = (nativeRaster.flags & HAS_ALPHA) != 0;
        }

        #if defined(RWDEBUG)
        NumTextureRasters++;
        #endif /* defined(RWDEBUG) */
    }
    else if (nativeRaster.format &
             ~(rwRASTERFORMATAUTOMIPMAP | rwRASTERFORMATMIPMAP)) /* Is an standard RW format? */
    {
        /* Create a raster */
        raster = RwRasterCreate(nativeRaster.width,
                                nativeRaster.height,
                                (RwInt32)nativeRaster.depth,
                                nativeRaster.type | nativeRaster.format);
        if (!raster)
        {
            RWRETURN(FALSE);
        }

        /* Get the raster extension */
        rasExt = RASTEREXTFROMRASTER(raster);

        rasExt->compressed = 0;
    }
    else
    {
        // THIS CODE PATH NEVER WORKS.

        /* Create a raster */
        raster = RwD3D9RasterCreate(nativeRaster.width,
                                    nativeRaster.height,
                                    nativeRaster.d3dFormat,
                                    nativeRaster.type |
                                    (nativeRaster.format & (rwRASTERFORMATAUTOMIPMAP | rwRASTERFORMATMIPMAP)));
        if (!raster)
        {
            RWRETURN(FALSE);
        }

        /* Get the raster extension */
        rasExt = RASTEREXTFROMRASTER(raster);
    }

    /* check automipmap support */
    if (nativeRaster.flags & USE_AUTOMIPMAPGEN)
    {
        if (rasExt->automipmapgen == 0)
        {
            RwDebugSendMessage(rwDEBUGMESSAGE, "_rwD3D9NativeTextureRead",
                               "Auto mipmap generation not supported.");

            RwRasterDestroy(raster);

            RWRETURN(FALSE);
        }
    }

    /* Check format and size */
    if (nativeRaster.format == (RwUInt32)RwRasterGetFormat(raster) &&
        nativeRaster.d3dFormat == rasExt->d3dFormat &&
        nativeRaster.width == raster->width &&
        nativeRaster.height == raster->height)
    {
        RwUInt32    autoMipmap, face, numMipLevels, i;

        /* Load the palette if palletized */
        if (nativeRaster.format & rwRASTERFORMATPAL4)
        {
            RwUInt8     *palette;
            RwUInt32    size;

            palette = RwRasterLockPalette(raster, rwRASTERLOCKWRITE);
            RWASSERT(NULL != palette);

            size = sizeof(PALETTEENTRY) * 32;
            if (RwStreamRead(stream, (void *)palette, size) != size)
            {
                RwRasterUnlockPalette(raster);

                RwRasterDestroy(raster);

                RWRETURN(FALSE);
            }

            RwRasterUnlockPalette(raster);
        }
        else if (nativeRaster.format & rwRASTERFORMATPAL8)
        {
            RwUInt8     *palette;
            RwUInt32    size;

            palette = RwRasterLockPalette(raster, rwRASTERLOCKWRITE);
            RWASSERT(NULL != palette);

            size = sizeof(PALETTEENTRY) * 256;
            if (RwStreamRead(stream, (void *)palette, size) != size)
            {
                RwRasterUnlockPalette(raster);

                RwRasterDestroy(raster);

                RWRETURN(FALSE);
            }

            RwRasterUnlockPalette(raster);
        }

        /* Remove AUTOMIPMAP flag to avoid unlock invoking mipmap create */
        autoMipmap = raster->cFormat & (rwRASTERFORMATAUTOMIPMAP >> 8);
        raster->cFormat &= ~autoMipmap;

        /* Prepare support for cube textures */
        if (rasExt->cube)
        {
            face = 6;
        }
        else
        {
            face = 1;
        }

        if (rasExt->automipmapgen)
        {
            numMipLevels = 1; /* Read only first level */
        }
        else
        {
            numMipLevels = nativeRaster.numMipLevels;
        }

        rasExt->face = 0;

        /* Load mips into raster */
        do
        {
            for (i = 0; i < numMipLevels; i++)
            {
                RwUInt8     *pixels;
                RwUInt32    size;

                pixels = RwRasterLock(raster, (RwUInt8)i, rwRASTERLOCKWRITE);
                RWASSERT(NULL != pixels);

                /* Size in bytes of mip to load */
                if (RwStreamRead(stream, (void *)&size, sizeof(RwUInt32)) != sizeof(RwUInt32))
                {
                    RwRasterUnlock(raster);

                    RwRasterDestroy(raster);

                    RWRETURN(FALSE);
                }

#ifdef RW_BRANCH_COMMDBG
                RwUInt32 maxvideobufsize = _rwD3D9NativeTextureGetVideoBufferSize( raster, rasExt, i );

                RwUInt32 allowload = min(maxvideobufsize, size);

                if (allowload > 0)
                {
                    if (RwStreamRead( stream, pixels, allowload ) != allowload)
                    {
                        RwRasterUnlock(raster);

                        RwRasterDestroy(raster);

                        RWRETURN(FALSE);
                    }
                }

                RwUInt32 skipcnt = ( size - allowload );

                if ( skipcnt > 0 )
                {
                    RwStreamSkip( stream, skipcnt );
                }
#else
                /* Read the mip level */
                if (RwStreamRead(stream, (void *)pixels, size) != size)
                {
                    RwRasterUnlock(raster);

                    RwRasterDestroy(raster);

                    RWRETURN(FALSE);
                }
#endif

                RwRasterUnlock(raster);
            }

            if (rasExt->cube)
            {
                rasExt->face += 1;
            }
        }
        while(--face);

        /* Restore auto mip flag */
        raster->cFormat |= autoMipmap;
    }
    else
    {
        RwDebugSendMessage(rwDEBUGMESSAGE, "_rwD3D9NativeTextureRead",
                            "Texture format or dimensions not supported.");

        RwRasterDestroy(raster);

        RWRETURN(FALSE);
    }

    texture = RwTextureCreate(raster);
    if (!texture)
    {
        RwRasterDestroy(raster);

        RWRETURN(FALSE);
    }

    RwTextureSetFilterMode(texture, nativeTexture.filterAndAddress & 0xFF);
    RwTextureSetAddressingU(texture, (nativeTexture.filterAndAddress >> 8) & 0x0F);
    RwTextureSetAddressingV(texture, (nativeTexture.filterAndAddress >> 12) & 0x0F);
    RwTextureSetName(texture, nativeTexture.name);
    RwTextureSetMaskName(texture, nativeTexture.mask);

    *((RwTexture **)textureIn) = texture;

    RWRETURN(TRUE);
}
