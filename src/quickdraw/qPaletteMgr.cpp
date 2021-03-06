/* Copyright 1994, 1995, 1996 by Abacus Research and
 * Development, Inc.  All rights reserved.
 */

/* Palette Manager */

#include <base/common.h>
#include <QuickDraw.h>
#include <WindowMgr.h>
#include <CQuickDraw.h>
#include <MemoryMgr.h>
#include <ResourceMgr.h>

#include <quickdraw/cquick.h>
#include <wind/wind.h>
#include <res/resource.h>
#include <mman/mman.h>
#include <vdriver/vdriver.h>
#include <vdriver/dirtyrect.h>
#include <algorithm>

using namespace Executor;

#define GD_CLUT_P(gd) (GD_TYPE(gd) == clutType)

#define CI_ALLOCATED_BIT 0x80000000

#define CI_ALLOCATED_ENTRY_P(entry) ((entry)->ciPrivate & CI_ALLOCATED_BIT)
#define CI_SET_ALLOCATED_ENTRY(entry, index) \
    ((entry)->ciPrivate = CI_ALLOCATED_BIT | (((index)&0xFF) << 16))
#define CI_DEALLOCATE_ENTRY(entry) \
    ((entry)->ciPrivate = 0)
#define CI_ENTRY_INDEX(entry) \
    (((entry)->ciPrivate >> 16) & 0xFF)
#define CI_USAGE(entry) ((entry)->ciUsage)
#define CI_TOLERANCE(entry) ((entry)->ciTolerance)
#define CI_USAGE_HAS_BITS_P(entry, bits) \
    ((CI_USAGE(entry) & 0xF) == (bits))
#define CI_RGB(entry) ((entry)->ciRGB)

#define PALETTE_MODIFIED_BIT (0x8000)
#define PALETTE_MODIFIED_BIT (0x8000)
#define PALETTE_MODIFIED_P(palette) \
    ((*palette)->pmPrivate & PALETTE_MODIFIED_BIT)
#define PALETTE_SET_MODIFIED(palette) \
    ((*palette)->pmPrivate |= PALETTE_MODIFIED_BIT)
#define PALETTE_CLEAR_MODIFIED(palette) \
    ((*palette)->pmPrivate &= ~PALETTE_MODIFIED_BIT)

#define PALETTE_SEED(palette) \
    (*(GUEST<LONGINT> *)*(*palette)->pmSeeds)

#define ELT_FREE_P(elt)                    \
    (!((elt)->value & (CTAB_RESERVED_BIT \
                       | CTAB_TOLERANT_BIT)))
#define ELT_ANIMATED_P(elt) \
    (((elt)->value & CTAB_RESERVED_BIT) == CTAB_RESERVED_BIT)

typedef struct pm_resource_holder
{
    PaletteHandle palette;
    int entry;

    GUEST<INTEGER> default_value;
    GUEST<RGBColor> default_rgb;
} pm_resource_holder_t;

/* device at position `n' corresponds to the resource holder list at
   index `n' */
static GDHandle *device_to_resholder;

static pm_resource_holder_t **pm_resource_holders;

static int n_resource_holders, max_resource_holders;

pm_resource_holder_t *
lookup_pm_resource_holders(void)
{
    int null_index = -1;
    int index;
    int i;

    for(i = 0; i < n_resource_holders; i++)
    {
        if(device_to_resholder[i] == LM(TheGDevice))
            return pm_resource_holders[i];
        else if(device_to_resholder[i] == nullptr)
            null_index = i;
    }

    /* new one */
    if(pm_resource_holders == nullptr)
    {
        max_resource_holders = 4;
        pm_resource_holders
            = (pm_resource_holder_t **)realloc(pm_resource_holders, (max_resource_holders
                                                                     * sizeof *pm_resource_holders));
        device_to_resholder
            = (GDHandle *)realloc(device_to_resholder, (max_resource_holders
                                                        * sizeof *device_to_resholder));
        index = n_resource_holders++;
    }
    else if(n_resource_holders == max_resource_holders
            && null_index == -1)
    {
        max_resource_holders *= 2;
        pm_resource_holders
            = (pm_resource_holder_t **)realloc(pm_resource_holders, (max_resource_holders
                                                                     * sizeof *pm_resource_holders));
        device_to_resholder
            = (GDHandle *)realloc(device_to_resholder, (max_resource_holders
                                                        * sizeof *device_to_resholder));
        index = n_resource_holders++;
    }
    else
    {
        index = null_index;
    }

    device_to_resholder[index] = LM(TheGDevice);
    pm_resource_holders[index]
        = (pm_resource_holder_t *)malloc(256 * sizeof *pm_resource_holders[index]);

    return pm_resource_holders[index];
}

void delete_pm_resource_holder(GDHandle gd)
{
    int i;

    for(i = 0; i < n_resource_holders; i++)
    {
        /* no swap */
        if(device_to_resholder[i] == gd)
        {
            device_to_resholder[i] = nullptr;
            free(pm_resource_holders[i]);
        }
    }
}

/* FIXME:
   make sure to send windows update events when
   reserving entries IMVI 20-23 */

/* associates a palette which each window; if the window is color and
   not in this list, it is associated with the default palette */
typedef struct window_palette_alist
{
    WindowPtr w;
    PaletteHandle palette;

    int c_update;

    struct window_palette_alist *next;
} * window_palette_alist_t;

static window_palette_alist_t window_palette_alist;
static window_palette_alist_t free_list;

static inline window_palette_alist_t
window_palette_alist_elt(WindowPtr w)
{
    window_palette_alist_t elt;

    for(elt = window_palette_alist; elt; elt = elt->next)
        if(elt->w == w)
            return elt;

    return nullptr;
}

/* return the current `implicit' palette */
PaletteHandle
get_current_palette(void)
{
    window_palette_alist_t elt;

    elt = window_palette_alist_elt((WindowPtr)qdGlobals().thePort);
    if(!elt)
        elt = window_palette_alist_elt(FrontWindow());
    return elt ? elt->palette : nullptr;
}

int window_p(WindowPtr w)
{
    WindowPeek t_w;

    for(t_w = LM(WindowList);
        t_w;
        t_w = WINDOW_NEXT_WINDOW(t_w))
    {
        if((WindowPtr)t_w == w)
            return true;
    }
    return false;
}

INTEGER Executor::C_PMgrVersion()
{
    return 0x200; /* original 32-bit QD system */
#if 0
  return 0x201; /* system software 6.0.5 */
  return 0x202; /* system software 7.0 */
#endif
}

static inline int
rgb_delta(RGBColor *c1, RGBColor *c2)
{
    return std::max(std::max(std::abs(c1->red - c2->red),
                   std::abs(c1->green - c2->green)),
               std::abs(c1->blue - c2->blue));
}

int pm_allocate_animated_elt(ColorSpec *elt, int elt_i, ColorInfo *entry,
                             int force_p)
{
    elt->value = CTAB_RESERVED_BIT;
    elt->rgb = CI_RGB(entry);

    CI_SET_ALLOCATED_ENTRY(entry, elt_i);

    return true;
}

int pm_allocate_tolerant_elt(ColorSpec *elt, int elt_i, ColorInfo *entry,
                             int force_p)
{
    int delta;

    delta = rgb_delta(&elt->rgb, &CI_RGB(entry));
    if(force_p
       || delta > CI_TOLERANCE(entry))
    {
        elt->value = CTAB_TOLERANT_BIT;
        elt->rgb = CI_RGB(entry);

        CI_SET_ALLOCATED_ENTRY(entry, elt_i);
        return true;
    }

    return false;
}

void pm_deallocate_entry(ColorInfo *entry, int change_color_env_p)
{
    PixMapHandle gd_pixmap;
    CTabHandle gd_ctab;
    ColorSpec *gd_ctab_table;
    pm_resource_holder_t *holder;
    pm_resource_holder_t *holders;
    ColorSpec *elt;
    int index;

    if(!CI_ALLOCATED_ENTRY_P(entry))
        return;

    holders = lookup_pm_resource_holders();

    index = CI_ENTRY_INDEX(entry);

    gd_pixmap = GD_PMAP(LM(TheGDevice));
    gd_ctab = PIXMAP_TABLE(gd_pixmap);
    gd_ctab_table = CTAB_TABLE(gd_ctab);

    holder = &holders[index];
    elt = &gd_ctab_table[index];

    if(change_color_env_p)
    {
        elt->value = holder->default_value;
        elt->rgb = holder->default_rgb;
    }
    else
        elt->value = CTAB_PENDING_BIT;

    CI_DEALLOCATE_ENTRY(entry);
    holder->palette = nullptr;
    holder->entry = -1;
}

int higher_priority_p(ColorInfo *entry0, int entry0_index,
                      ColorInfo *entry1, int entry1_index)
{
    bool entry0_explicit, entry0_animated, entry0_tolerant;
    bool entry1_explicit, entry1_animated, entry1_tolerant;

    entry0_explicit = (CI_USAGE(entry0) & pmExplicit) != 0;
    entry0_animated = (CI_USAGE(entry0) & pmAnimated) != 0;
    entry0_tolerant = (CI_USAGE(entry0) & pmTolerant) != 0;

    entry1_explicit = (CI_USAGE(entry1) & pmExplicit) != 0;
    entry1_animated = (CI_USAGE(entry1) & pmAnimated) != 0;
    entry1_tolerant = (CI_USAGE(entry1) & pmTolerant) != 0;

    /* explicit takes precidence over non-explicit */
    if(entry0_explicit && !entry1_explicit)
        return true;
    else if(!entry0_explicit && entry1_explicit)
        return false;
    else if(entry0_explicit && entry1_explicit)
    {
        if(entry0_animated && entry1_tolerant)
            return true;
        else if(entry0_tolerant && entry1_animated)
            return false;
        else
            return entry0_index < entry1_index;
    }
    /* both non-explicit */
    else if(entry0_animated && entry1_tolerant)
        return true;
    else if(entry0_tolerant && entry1_animated)
        return false;
    else
        return entry0_index < entry1_index;
}

#define allocator(bits, elt, elt_i, entry, entry_i, force_p)                    \
    do {                                                                        \
        int allocated_p = false;                                                \
                                                                                \
        if((bits)&pmAnimated)                                                   \
            allocated_p = pm_allocate_animated_elt(elt, elt_i, entry, force_p); \
        else if((bits)&pmTolerant)                                              \
            allocated_p = pm_allocate_tolerant_elt(elt, elt_i, entry, force_p); \
                                                                                \
        if(allocated_p)                                                         \
        {                                                                       \
            pm_resource_holder_t *holder;                                       \
                                                                                \
            holder = &holders[elt_i];                                           \
            holder->palette = palette;                                          \
            holder->entry = entry_i;                                            \
                                                                                \
            gd_ctab_changed_p = true;                                           \
        }                                                                       \
    } while(0)

#define EXPLICIT_LOOP(bits)                                                         \
    do {                                                                            \
        for(i = 0; i < entries; i++)                                                \
        {                                                                           \
            ColorInfo *entry;                                                       \
            ColorSpec *elt;                                                         \
            int force_p = 0;                                                        \
            pm_resource_holder_t *holder;                                           \
                                                                                    \
            entry = &palette_info[i];                                               \
            elt = &gd_ctab_table[i & gd_index_mask];                                \
            /* if this entry is inhibited on this device, go to the next */         \
            if((CI_USAGE(entry) & gd_inhibit_flag)                                  \
               || CI_ALLOCATED_ENTRY_P(entry)                                       \
               || !CI_USAGE_HAS_BITS_P(entry, bits)                                 \
                                                                                    \
               /* it is not possible to reserve the first or last ctab              \
                  entries */                                                        \
               || (i & gd_index_mask) == 0                                          \
               || (i & gd_index_mask) == gd_ctab_size)                              \
                continue;                                                           \
            holder = &holders[i & gd_index_mask];                                   \
            if(elt->value & CTAB_RESERVED_BIT)                                      \
            {                                                                       \
                if(holder->palette)                                                 \
                {                                                                   \
                    ColorInfo *prev_entry;                                          \
                                                                                    \
                    prev_entry = &PALETTE_INFO(holder->palette)[holder->entry];     \
                    if(holder->palette == palette                                   \
                       && higher_priority_p(prev_entry, holder->entry,              \
                                            entry, i))                              \
                        continue;                                                   \
                    CI_DEALLOCATE_ENTRY(prev_entry);                                \
                }                                                                   \
                else                                                                \
                {                                                                   \
                    /* to restore when we are done with it, what happens if         \
                   the ColorMgr frees up this slot in the meantime? */              \
                    holder->default_value = elt->value;                             \
                    holder->default_rgb = elt->rgb;                                 \
                }                                                                   \
                force_p = 1;                                                        \
            }                                                                       \
            else if(elt->value & CTAB_TOLERANT_BIT)                                 \
            {                                                                       \
                ColorInfo *prev_entry;                                              \
                                                                                    \
                gui_assert(holder->palette);                                        \
                prev_entry = &PALETTE_INFO(holder->palette)[holder->entry];         \
                if(holder->palette == palette)                                      \
                {                                                                   \
                    warning_unexpected("attempting to reallocate allocated entry"); \
                    continue;                                                       \
                }                                                                   \
                else if(higher_priority_p(prev_entry, holder->entry,                \
                                          entry, i))                                \
                    continue;                                                       \
                                                                                    \
                CI_DEALLOCATE_ENTRY(prev_entry);                                    \
                                                                                    \
                force_p = 1;                                                        \
            }                                                                       \
            allocator(bits, elt, i &gd_index_mask, entry, i, force_p);              \
        }                                                                           \
    } while(false)

#define ANIMATED_LOOP(bits)                                                         \
    do {                                                                            \
        int free_elt_i, steal_elt_i;                                                \
                                                                                    \
        /* start with 1, it is not possible to reserve the first or last            \
           ctab elts */                                                             \
        free_elt_i = 1;                                                             \
        steal_elt_i = 1;                                                            \
                                                                                    \
        /* it is not possible to reserve the first or last entries */               \
        for(i = 0; i < entries; i++)                                                \
        {                                                                           \
            ColorInfo *entry;                                                       \
                                                                                    \
            entry = &palette_info[i];                                               \
            /* if this entry is inhibited on this device, go to the next */         \
            if((CI_USAGE(entry) & gd_inhibit_flag)                                  \
               || CI_ALLOCATED_ENTRY_P(entry)                                       \
               || !CI_USAGE_HAS_BITS_P(entry, bits))                                \
                continue;                                                           \
                                                                                    \
            for(; free_elt_i <= (gd_ctab_size - 1); free_elt_i++)                   \
            {                                                                       \
                ColorSpec *free_elt;                                                \
                                                                                    \
                free_elt = &gd_ctab_table[free_elt_i];                              \
                if(ELT_FREE_P(free_elt))                                            \
                {                                                                   \
                    allocator(bits, free_elt, free_elt_i, entry, i, false);         \
                    free_elt_i++;                                                   \
                    goto next_entry;                                                \
                }                                                                   \
            }                                                                       \
                                                                                    \
            /* theft should be prioritized so we steal tolerant and explicit        \
           tolerant (?) entries before animated entries */                          \
            for(; steal_elt_i <= (gd_ctab_size - 1); steal_elt_i++)                 \
            {                                                                       \
                pm_resource_holder_t *holder;                                       \
                ColorSpec *elt;                                                     \
                ColorInfo *prev_entry = nullptr;                                    \
                                                                                    \
                elt = &gd_ctab_table[steal_elt_i];                                  \
                holder = &holders[steal_elt_i];                                     \
                                                                                    \
                if(holder->palette)                                                 \
                    prev_entry = &PALETTE_INFO(holder->palette)[holder->entry];     \
                                                                                    \
                if(holder->palette == palette)                                      \
                {                                                                   \
                    warning_unexpected("attempting to reallocate allocated entry"); \
                    continue;                                                       \
                }                                                                   \
                else if(higher_priority_p(prev_entry, holder->entry,                \
                                          entry, i))                                \
                    continue;                                                       \
                                                                                    \
                if(holder->palette == nullptr)                                      \
                {                                                                   \
                    /* to restore when we are done with it, what happens if         \
                   the ColorMgr frees up this slot in the meantime? */              \
                    holder->default_value = elt->value;                             \
                    holder->default_rgb = elt->rgb;                                 \
                }                                                                   \
                else                                                                \
                    CI_DEALLOCATE_ENTRY(prev_entry);                                \
                                                                                    \
                allocator(bits, elt, steal_elt_i, entry, i, true);                  \
                steal_elt_i++;                                                      \
                break;                                                              \
            }                                                                       \
        next_entry:;                                                                \
        }                                                                           \
    } while(false)

/* for each tolerant entry, find the closest free color table entry,
   and if it isn't close enough, smash it

   this is very inefficient currently.  what we should do is rebuild
   the inverse table after the other passes, then if the closest match
   is not close enough, steal the (an) entry */
#define TOLERANT_LOOP(bits)                                                 \
    do {                                                                    \
        for(i = 0; i < entries; i++)                                        \
        {                                                                   \
            ColorInfo *entry;                                               \
            int closest_elt_i = -1, closest_delta;                          \
            int elt_i;                                                      \
            ColorSpec *closest_elt = nullptr;                               \
                                                                            \
            entry = &palette_info[i];                                       \
            /* if this entry is inhibited on this device, go to the next */ \
            if((CI_USAGE(entry) & gd_inhibit_flag)                          \
               || CI_ALLOCATED_ENTRY_P(entry)                               \
               || !CI_USAGE_HAS_BITS_P(entry, bits))                        \
                continue;                                                   \
                                                                            \
            closest_delta = CI_TOLERANCE(entry);                            \
                                                                            \
            for(elt_i = 0; elt_i <= gd_ctab_size; elt_i++)                  \
            {                                                               \
                ColorSpec *elt;                                             \
                int delta;                                                  \
                                                                            \
                elt = &gd_ctab_table[elt_i];                                \
                if(ELT_ANIMATED_P(elt))                                     \
                    continue;                                               \
                                                                            \
                delta = rgb_delta(&elt->rgb, &CI_RGB(entry));               \
                if(delta <= closest_delta)                                  \
                {                                                           \
                    closest_elt_i = i;                                      \
                    closest_delta = delta;                                  \
                    closest_elt = elt;                                      \
                                                                            \
                    if(!delta)                                              \
                        break;                                              \
                }                                                           \
            }                                                               \
                                                                            \
            if(closest_elt == nullptr)                                         \
            {                                                               \
                /* can't allocate first or last entry */                    \
                for(elt_i = 1; elt_i <= (gd_ctab_size - 1); elt_i++)        \
                {                                                           \
                    ColorSpec *elt;                                         \
                                                                            \
                    elt = &gd_ctab_table[elt_i];                            \
                                                                            \
                    if(ELT_FREE_P(elt))                                     \
                    {                                                       \
                        /* take the first free elt */                       \
                        allocator(bits, elt, elt_i, entry, i, false);       \
                        break;                                              \
                    }                                                       \
                }                                                           \
            }                                                               \
        }                                                                   \
    } while(false)

static void
pm_do_updates_gd_changed(void)
{
    GDHandle gd;
    PixMapHandle gd_pixmap;
    CTabHandle gd_ctab;

    window_palette_alist_t t;
    WindowPtr front_w;

    gd = LM(TheGDevice);

    if(!GD_CLUT_P(gd))
        /* no updates to do here */
        return;

    gd_pixmap = GD_PMAP(gd);
    gd_ctab = PIXMAP_TABLE(gd_pixmap);

    CTAB_SEED(gd_ctab) = GetCTSeed();

    LM(PaintWhite) = 0;
    front_w = FrontWindow();
    for(t = window_palette_alist; t; t = t->next)
    {
        if(t->w == (WindowPtr)-1)
        {
            PaintOne((WindowPeek)nullptr, LM(GrayRgn));
        }
        else if(!window_p(t->w))
            continue;
        else if((t->w == front_w
                 && (t->c_update & pmFgUpdates) == pmFgUpdates)
                || (t->w != front_w
                    && (t->c_update & pmBkUpdates) == pmBkUpdates))
        {
            PaintOne((WindowPeek)t->w, WINDOW_STRUCT_REGION(t->w));
        }
    }
    LM(PaintWhite) = -1;

    dirty_rect_update_screen();
    vdriver->setColors(0, CTAB_SIZE(gd_ctab) + 1, CTAB_TABLE(gd_ctab));
}

#define gd_index_mask gd_ctab_size

static WindowPtr cached_front_window = nullptr;

void Executor::C_ActivatePalette(WindowPtr src_window)
{
    pm_resource_holder_t *holders;
    PaletteHandle palette;
    GDHandle gd;
    PixMapHandle gd_pixmap;
    int gd_ctab_changed_p = false;
    CTabHandle gd_ctab;
    ColorSpec *gd_ctab_table;
    int gd_ctab_size;
    /* number of entries in our palette */
    int entries;
    short gd_inhibit_flag;
    ColorInfo *palette_info;
    int gd_bpp;
    int i;

    palette = GetPalette(src_window);
    if(palette == nullptr)
        palette = GetPalette((WindowPtr)-1);

    cached_front_window = FrontWindow();
    if(src_window != cached_front_window)
        return;

    entries = PALETTE_ENTRIES(palette);
    palette_info = PALETTE_INFO(palette);

    gd = LM(TheGDevice);
    gd_pixmap = GD_PMAP(gd);
    gd_bpp = PIXMAP_PIXEL_SIZE(gd_pixmap);
    gd_ctab = PIXMAP_TABLE(gd_pixmap);
    gd_ctab_table = CTAB_TABLE(gd_ctab);
    gd_ctab_size = CTAB_SIZE(gd_ctab);

    if(!GD_CLUT_P(gd))
        /* `ActivatePalette ()' can't help you if it doesn't have a clut
       to work with */
        return;

    /* if neither the color environment or the palette has changed, we
     have nothing to do */
    if(!PALETTE_MODIFIED_P(palette)
       /* PM5.0a sets the seeds to `-1', as far as i can tell */
       && PALETTE_SEEDS(palette) != ptr_from_longint<Handle>(-1)
       /* we only have a single display currently */
       && PALETTE_SEED(palette) == CTAB_SEED(gd_ctab))
        return;

    /* FIXME: currently we only support color gdevices, no grayscale */
    switch(gd_bpp)
    {
        case 2:
            gd_inhibit_flag = pmInhibitC2;
            break;
        case 4:
            gd_inhibit_flag = pmInhibitC4;
            break;
        case 8:
            gd_inhibit_flag = pmInhibitC8;
            break;
        default:
            gd_inhibit_flag = 0;
            break;
    }

    holders = lookup_pm_resource_holders();

    /* go through the gd_ctab and find all `CTAB_PENDING_BIT' entries
     and `default'ify them */
    if(1)
    {
        for(i = 0; i <= gd_ctab_size; i++)
        {
            ColorSpec *elt;

            elt = &gd_ctab_table[i];
            if(elt->value & CTAB_PENDING_BIT)
            {
                pm_resource_holder_t *holder;
                holder = &holders[i];

                gui_assert(holder->palette == nullptr);

                /* install the default values */
                elt->value = holder->default_value;
                elt->rgb = holder->default_rgb;

                gd_ctab_changed_p = true;
            }
        }
    }

    /* prioritize entries */
    EXPLICIT_LOOP(pmAnimated + pmExplicit);
    EXPLICIT_LOOP(pmTolerant + pmExplicit);
    ANIMATED_LOOP(pmAnimated);
    TOLERANT_LOOP(pmTolerant);

    if(gd_ctab_changed_p)
        pm_do_updates_gd_changed();

    PALETTE_CLEAR_MODIFIED(palette);
    if(PALETTE_SEEDS(palette) != guest_cast<Handle>(-1))
        PALETTE_SEED(palette) = CTAB_SEED(gd_ctab);
}

void Executor::C_RestoreDeviceClut(GDHandle gd)
{
    bool gd_ctab_changed_p = false;
    pm_resource_holder_t *holders;
    PixMapHandle gd_pixmap;
    CTabHandle gd_ctab;
    int i;

    warning_unimplemented("RestoreDeviceClut implementation may be shaky.");

    if(gd == nullptr)
        gd = LM(MainDevice);

    if(gd != LM(MainDevice) || !GD_CLUT_P(gd))
        return;

    gd_pixmap = GD_PMAP(gd);
    gd_ctab = PIXMAP_TABLE(gd_pixmap);
    {
        HLockGuard guard(gd_ctab);
        ColorSpec *gd_ctab_table;
        int gd_ctab_size;

        gd_ctab_table = CTAB_TABLE(gd_ctab);
        gd_ctab_size = CTAB_SIZE(gd_ctab);
        holders = lookup_pm_resource_holders();

        for(i = 0; i <= gd_ctab_size; i++)
        {
            RGBColor *gd_rgb;
            RGBColor *holder_rgb;
            pm_resource_holder_t *holder;

            holder = &holders[i];

            /* reset palette manager state */
            if(holder->palette)
            {
                ColorInfo *entry;

                entry = &PALETTE_INFO(holder->palette)[holder->entry];
                pm_deallocate_entry(entry, true);

                gd_ctab_changed_p = true;
            }

            gd_rgb = &gd_ctab_table[i].rgb;
            holder_rgb = &holder->default_rgb;
            /* reset clut state */
            if(gd_ctab_table[i].value != holder->default_value
               || gd_rgb->red != holder_rgb->red
               || gd_rgb->green != holder_rgb->green
               || gd_rgb->blue != holder_rgb->blue)
            {
                gd_ctab_table[i].value = holder->default_value;
                gd_ctab_table[i].rgb = holder->default_rgb;
                gd_ctab_changed_p = true;
            }
        }
    }

    if(gd_ctab_changed_p)
        pm_do_updates_gd_changed();
}

void Executor::C_InitPalettes()
{
    ColorInfo *default_palette_info;
    PaletteHandle default_palette;
    pm_resource_holder_t *pm_resource_holders;
    window_palette_alist_t elt;
    int i;

    pm_resource_holders = lookup_pm_resource_holders();

    for(i = 0; i < 256; i++)
    {
        pm_resource_holders[i].palette = nullptr;
        pm_resource_holders[i].entry = -1;

        pm_resource_holders[i].default_value = 0;
        pm_resource_holders[i].default_rgb = ctab_8bpp_values[i].rgb;
    }

    default_palette = GetNewPalette(0);

    if(default_palette == nullptr)
    {
        ColorInfo *entry;

        default_palette
            = (PaletteHandle)(NewHandle(PALETTE_STORAGE_FOR_ENTRIES(2)));
        PALETTE_ENTRIES(default_palette) = 2;

        PALETTE_PRIVATE(default_palette) = 0;

        /* ### don't know what these fields fields are for, unitialized
 	 them to some random value */
        PALETTE_WINDOW(default_palette) = nullptr;
        PALETTE_DEVICES(default_palette) = -1;

        default_palette_info = PALETTE_INFO(default_palette);

        entry = &default_palette_info[0];
        entry->ciRGB = ROMlib_white_rgb_color;
        entry->ciUsage = pmCourteous;
        entry->ciTolerance = 0;
        entry->ciPrivate = 0;

        entry = &default_palette_info[1];
        entry->ciRGB = ROMlib_black_rgb_color;
        entry->ciUsage = pmCourteous;
        entry->ciTolerance = 0;
        entry->ciPrivate = 0;

        /* initial contents don't matter */
        PALETTE_SEEDS(default_palette) = NewHandle(sizeof(int));
        PALETTE_SET_MODIFIED(default_palette);
    }

    elt = (window_palette_alist_t)(NewPtr(sizeof *elt));
    elt->palette = default_palette;
    /* the default palette has a window id of -1 */
    elt->w = (WindowPtr)-1;
    elt->next = nullptr;

    window_palette_alist = elt;
    free_list = nullptr;
}

PaletteHandle Executor::C_NewPalette(INTEGER entries, CTabHandle src_colors,
                                     INTEGER src_usage, INTEGER src_tolerance)
{
    PaletteHandle new_palette;
    ColorInfo *info;
    int i;

    new_palette
        = (PaletteHandle)NewHandle(PALETTE_STORAGE_FOR_ENTRIES(entries));
    memset(*new_palette, 0, PALETTE_STORAGE_FOR_ENTRIES(entries));

    /* initial contents don't matter */
    PALETTE_SEEDS(new_palette) = NewHandle(sizeof(int));
    PALETTE_SET_MODIFIED(new_palette);

    PALETTE_ENTRIES(new_palette) = entries;
    info = PALETTE_INFO(new_palette);

    i = 0;
    if(src_colors)
    {
        ColorSpec *colors;
        int max_colors = CTAB_SIZE(src_colors);

        colors = CTAB_TABLE(src_colors);
        for(; i < entries && i <= max_colors; i++)
        {
            info[i].ciRGB = colors[i].rgb;
            info[i].ciUsage = src_usage;
            info[i].ciTolerance = src_tolerance;
        }
    }

    for(; i < entries; i++)
    {
        info[i].ciRGB = ROMlib_black_rgb_color;
        info[i].ciUsage = src_usage;
        info[i].ciTolerance = src_tolerance;
    }

    return new_palette;
}

PaletteHandle Executor::C_GetNewPalette(INTEGER id)
{
    /* since the resource for a palette is identical to the layout
     of a palette, the type of the palette resource is
     `PaletteHandle' */
    PaletteHandle palette_res_h, retval;
    int palette_size;

    palette_res_h = (PaletteHandle)ROMlib_getrestid("pltt"_4, id);
    if(!palette_res_h)
        return nullptr;

    palette_size
        = PALETTE_STORAGE_FOR_ENTRIES(PALETTE_ENTRIES(palette_res_h));
    retval = (PaletteHandle)(NewHandle(palette_size));
    BlockMoveData((Ptr)*palette_res_h, (Ptr)*retval,
                  palette_size);

    /* initial contents don't matter */
    PALETTE_SEEDS(retval) = NewHandle(sizeof(int));
    PALETTE_SET_MODIFIED(retval);

    return retval;
}

void Executor::pm_front_window_maybe_changed_hook(void)
{
    WindowPtr t_w;

    t_w = FrontWindow();
    if(cached_front_window != t_w)
    {
        cached_front_window = t_w;
        ActivatePalette(cached_front_window);
    }
}

/* called when window `w' is deleted */
void Executor::pm_window_closed(WindowPtr w)
{
    PaletteHandle palette;
    window_palette_alist_t t, prev;
    int deallocate_entries_p = true;
    int i;

    palette = GetPalette(w);
    if(palette == nullptr)
        palette = GetPalette((WindowPtr)-1);

    for(prev = nullptr, t = window_palette_alist; t; t = t->next)
    {
        if(t->w == w)
        {
            if(prev)
                prev->next = t->next;
            else
                window_palette_alist = t->next;
            t->next = free_list;
            free_list = t;
            t = prev ? prev : window_palette_alist;
        }
        else if(t->palette == palette)
            deallocate_entries_p = false;
        prev = t;
    }
    if(deallocate_entries_p)
    {
        for(i = PALETTE_ENTRIES(palette) - 1; i >= 0; i--)
        {
            ColorInfo *entry;

            entry = &PALETTE_INFO(palette)[i];
            pm_deallocate_entry(entry, true);
        }

        /* #### force the gd to be updated */
        pm_do_updates_gd_changed();

        /* #### color environment changed, so update the current
         palette, since the application won't necessarily know to do
         an ActivatePalette itself */
        ActivatePalette(cached_front_window);
    }
}

void Executor::C_DisposePalette(PaletteHandle palette)
{
    int i;

    /* remove the alist associations between windows and this palette */
    window_palette_alist_t t, prev;

    for(prev = nullptr, t = window_palette_alist; t;)
    {
        if(t->palette == palette)
        {
            if(prev)
            {
                prev->next = t->next;

                t->next = free_list;
                free_list = t;

                t = prev->next;
            }
            else
            {
                window_palette_alist = t->next;

                t->next = free_list;
                free_list = t;

                t = window_palette_alist;
            }
        }
        else
        {
            prev = t;
            t = t->next;
        }
    }

    for(i = PALETTE_ENTRIES(palette) - 1; i >= 0; i--)
    {
        ColorInfo *entry;

        entry = &PALETTE_INFO(palette)[i];
        pm_deallocate_entry(entry, false);
    }

    DisposeHandle((Handle)palette);
}

void Executor::C_ResizePalette(PaletteHandle palette, INTEGER new_size)
{
    ColorInfo *entries;
    int old_size;
    int i;

    old_size = PALETTE_ENTRIES(palette);
    /* deleting entries */
    if(old_size > new_size)
    {
        entries = PALETTE_INFO(palette);
        for(i = new_size; i < old_size; i++)
        {
            ColorInfo *entry;

            entry = &PALETTE_INFO(palette)[i];
            pm_deallocate_entry(entry, false);
        }
    }
    SetHandleSize((Handle)palette,
                  PALETTE_STORAGE_FOR_ENTRIES(new_size));
    /* new entries */
    if(new_size > old_size)
    {
        ColorInfo *entries;

        entries = PALETTE_INFO(palette);
        for(i = old_size; i < new_size; i++)
        {
            ColorInfo *entry;

            entry = &entries[i];
            entry->ciRGB = ROMlib_white_rgb_color;
            entry->ciUsage = pmCourteous;
            entry->ciFlags = entries->ciTolerance = 0;
            entry->ciPrivate = 0;
        }
    }
}

static void
set_palette_common(WindowPtr dst_window, PaletteHandle src_palette,
                   pmUpdates c_update)
{
    /* get a new window_palette_alist element; fill in the appropriate
     fields, and add it to the alist */
    window_palette_alist_t elt;

    if(src_palette == nullptr)
        return;

    elt = window_palette_alist_elt(dst_window);
    if(!elt)
    {
        if(!free_list)
            elt = (window_palette_alist_t)(NewPtr(sizeof *elt));
        else
        {
            elt = free_list;
            free_list = free_list->next;
        }

        elt->palette = nullptr;

        /* place the new element onto the window_palette_alist */
        elt->next = window_palette_alist;
        window_palette_alist = elt;
    }

    elt->w = dst_window;
    if(elt->palette)
    {
        PaletteHandle palette = elt->palette;
        window_palette_alist_t t;
        int i;

        /* see if another window uses this palette; if not, deallocate
         this palette's entries */
        for(t = window_palette_alist; t; t = t->next)
        {
            if(t != elt && t->palette == elt->palette)
                goto after_deallocate_entries;
        }

        for(i = PALETTE_ENTRIES(palette) - 1; i >= 0; i--)
        {
            ColorInfo *entry;

            entry = &PALETTE_INFO(palette)[i];
            pm_deallocate_entry(entry, true);
        }

        /* ##### force the gd to be updated */
        pm_do_updates_gd_changed();

    after_deallocate_entries:;
    }

    elt->palette = src_palette;

#if 0
  /* clear the palette bits, and set the new update */
  PALETTE_PRIVATE (src_palette) &= ~PALETTE_UPDATE_FLAG_BITS;
  PALETTE_PRIVATE (src_palette) |= c_update;
#else
    elt->c_update = c_update;
#endif
    /* FIXME: hack, i don't know if this is right, but lemmings creates
     a window, sets the palette, and goes.  never calls `SelectWindow ()',
     or anything */
    if(dst_window == FrontWindow())
        ActivatePalette(dst_window);
}

void Executor::C_SetPalette(WindowPtr dst_window, PaletteHandle src_palette,
                            Boolean c_update)
{
    set_palette_common(dst_window, src_palette,
                       c_update ? pmAllUpdates : pmNoUpdates);
}

void Executor::C_NSetPalette(WindowPtr dst_window, PaletteHandle src_palette,
                             INTEGER nc_update)
{
    set_palette_common(dst_window, src_palette,
                       (pmUpdates)(nc_update & 0xff ? pmAllUpdates : nc_update));
}

void Executor::C_SetPaletteUpdates(PaletteHandle palette, INTEGER update)
{
    PALETTE_PRIVATE(palette) &= ~PALETTE_UPDATE_FLAG_BITS;
    PALETTE_PRIVATE(palette) |= update;
}

INTEGER Executor::C_GetPaletteUpdates(PaletteHandle palette)
{
    return PALETTE_PRIVATE(palette) & PALETTE_UPDATE_FLAG_BITS;
}

PaletteHandle Executor::C_GetPalette(WindowPtr src_window)
{
    window_palette_alist_t elt;

    elt = window_palette_alist_elt(src_window);
    if(elt)
        return elt->palette;
    return nullptr;
}

#define pm_xxx_color(index_macro_x, rgb_macro, rgb_fn, entry)                          \
    {                                                                                  \
        PaletteHandle palette;                                                         \
        ColorInfo *info;                                                               \
                                                                                       \
        palette = get_current_palette();                                               \
        if(!palette)                                                                   \
            palette = GetPalette((WindowPtr)-1);                                       \
        if((entry) < 0 || (entry) > PALETTE_ENTRIES(palette))                          \
        {                                                                              \
            warning_unexpected("Out of bounds palette entry %d.", (entry));            \
            return;                                                                    \
        }                                                                              \
        info = &PALETTE_INFO(palette)[(entry)];                                        \
                                                                                       \
        if(CI_USAGE(info) & pmExplicit)                                         \
        {                                                                              \
            int gd_index_mask;                                                         \
                                                                                       \
            gd_index_mask = CTAB_SIZE(PIXMAP_TABLE(GD_PMAP(LM(TheGDevice))));          \
            index_macro_x(qdGlobals().thePort) = (entry)&gd_index_mask;                        \
        }                                                                              \
        else if(CI_USAGE(info) & pmAnimated)                                    \
        {                                                                              \
            if(CI_ALLOCATED_ENTRY_P(info))                                             \
            {                                                                          \
                index_macro_x(qdGlobals().thePort) = CI_ENTRY_INDEX(info);                       \
                /* this necessary? */                                                  \
                rgb_macro(qdGlobals().thePort) = CI_RGB(info);                                     \
            }                                                                          \
            else                                                                       \
            {                                                                          \
                /* FIXME: not sure what to do about this.  if it is an                 \
                   explicit entry, should i set appropriate color */                   \
                warning_unexpected("attempt to `Pm..Color ()' a unallocated index %d", \
                                   entry);                                             \
            }                                                                          \
        }                                                                              \
        else if((CI_USAGE(info) & pmTolerant)                                   \
                || ((CI_USAGE(info)                                                  \
                     & CI_USAGE_TYPE_BITS)                                           \
                    == pmCourteous))                                              \
        {                                                                              \
            rgb_fn(&CI_RGB(info));                                                     \
        }                                                                              \
        else                                                                           \
            warning_unexpected("unknown usage type");                                  \
    }

void Executor::C_PmForeColor(INTEGER entry)
{
    pm_xxx_color(PORT_FG_COLOR, CPORT_RGB_FG_COLOR, RGBForeColor,
                 (int)entry);
}

void Executor::C_PmBackColor(INTEGER entry)
{
    pm_xxx_color(PORT_BK_COLOR, CPORT_RGB_BK_COLOR, RGBBackColor,
                 (int)entry);
}

enum
{
    useRGB = 0,
    usePM = 1
};

void Executor::C_SaveFore(ColorSpec *cp)
{
    warning_unimplemented("always uses RGB");

    GetForeColor(&cp->rgb);
    cp->value = useRGB;
}

void Executor::C_RestoreFore(ColorSpec *cp)
{
    switch(cp->value)
    {
        case usePM:
            warning_unimplemented("using rgb, even though pm was requested");
            goto USE_RGB_ANYWAY;
            break;
        default:
            warning_unexpected("value = 0x%x (using rgb)", toHost(cp->value));
        /* FALL THROUGH */
        case useRGB:
        USE_RGB_ANYWAY:
            RGBForeColor(&cp->rgb);
            break;
    }
}

void Executor::C_SaveBack(ColorSpec *cp)
{
    warning_unimplemented("always uses RGB");

    GetBackColor(&cp->rgb);
    cp->value = useRGB;
}

void Executor::C_RestoreBack(ColorSpec *cp)
{
    switch(cp->value)
    {
        case usePM:
            warning_unimplemented("using rgb, even though pm was requested");
            goto USE_RGB_ANYWAY;
            break;
        default:
            warning_unexpected("value = 0x%x (using rgb)", toHost(cp->value));
        /* FALL THROUGH */
        case useRGB:
        USE_RGB_ANYWAY:
            RGBBackColor(&cp->rgb);
            break;
    }
}

static int update_host_colors_p = true;

void Executor::C_AnimateEntry(WindowPtr dst_window, INTEGER dst_entry,
                              RGBColor *src_rgb_color)
{
    PalettePtr palette;
    PaletteHandle dst_palette_h;
    ColorInfo *entry;

    dst_palette_h = GetPalette(dst_window);
    if(!dst_palette_h)
        dst_palette_h = GetPalette((WindowPtr)-1);
    palette = *dst_palette_h;
    entry = &palette->pmInfo[dst_entry];

    /* do nothing if the entry is not animated */
    if((CI_USAGE(entry) & pmAnimated) != pmAnimated)
        return;

    CI_RGB(entry) = *src_rgb_color;

    if(CI_ALLOCATED_ENTRY_P(entry))
    {
        int ctab_index;
        GDHandle gdev;
        RGBColor *r;
        CTabHandle gd_ctab;

        /* FIXME - this should really loop over all devices which
       * use this window's palette.  It's not obvious to me how to do
       * this, so for now we'll assume the window's palette is
       * used by LM(TheGDevice).  This is fairly reasonable.
       */
        gdev = LM(TheGDevice);
        gd_ctab = PIXMAP_TABLE(GD_PMAP(gdev));
        ctab_index = CI_ENTRY_INDEX(entry);

        /* Set up the new color.  If it changed, we'll update the
       * host's screen.
       */
        r = &CTAB_TABLE(gd_ctab)[ctab_index].rgb;
        if(r->red != src_rgb_color->red
           || r->green != src_rgb_color->green
           || r->blue != src_rgb_color->blue)
        {
            *r = *src_rgb_color;
            if(update_host_colors_p)
            {
                dirty_rect_update_screen();
                vdriver->setColors(0, CTAB_SIZE(gd_ctab) + 1,
                                   CTAB_TABLE(gd_ctab));
            }
        }
    }
}

void Executor::C_AnimatePalette(WindowPtr dst_window, CTabHandle src_ctab,
                                INTEGER src_index, INTEGER dst_entry,
                                INTEGER dst_length)
{
    PaletteHandle dst_window_palette_h;
    PalettePtr palette;
    ColorSpec *src_cspec;
    Boolean save_update;
    int i;

    dst_window_palette_h = GetPalette(dst_window);
    if(dst_window_palette_h == nullptr)
        dst_window_palette_h = GetPalette((WindowPtr)-1);

    palette = *dst_window_palette_h;

    /* Compute the number of entries in the table to modify. */
    dst_length = std::min<INTEGER>((CTAB_SIZE(src_ctab) + 1) - src_index, dst_length);
    dst_length = std::min<INTEGER>(palette->pmEntries - dst_entry, dst_length);

    src_cspec = &CTAB_TABLE(src_ctab)[src_index];

    /* Animate all of the entries. */
    save_update = update_host_colors_p;
    update_host_colors_p = false; /* Avoid updating host for each entry. */
    for(i = 0; i < dst_length; i++)
        AnimateEntry(dst_window, dst_entry + i, &src_cspec[i].rgb);
    update_host_colors_p = save_update;

    /* To be safe, update the colors visible on the screen. */
    {
        CTabHandle ctab;
        dirty_rect_update_screen();
        ctab = PIXMAP_TABLE(GD_PMAP(LM(TheGDevice)));
        vdriver->setColors(0, CTAB_SIZE(ctab) + 1, CTAB_TABLE(ctab));
    }
}

void Executor::C_GetEntryColor(PaletteHandle src_palette, INTEGER entry_index,
                               RGBColor *dst_rgb_color)
{
    ColorInfo *entry;

    entry = &PALETTE_INFO(src_palette)[entry_index];
    *dst_rgb_color = entry->ciRGB;
}

void Executor::C_SetEntryColor(PaletteHandle dst_palette, INTEGER entry_index,
                               RGBColor *src_rgb_color)
{
    ColorInfo *entry;

    if(entry_index >= PALETTE_ENTRIES(dst_palette))
    {
        warning_unexpected("attempt to set entry beyond end of palette");
        return;
    }
    entry = &PALETTE_INFO(dst_palette)[entry_index];
    CI_RGB(entry) = *src_rgb_color;

    pm_deallocate_entry(entry, false);

    PALETTE_SET_MODIFIED(dst_palette);
}

void Executor::C_GetEntryUsage(PaletteHandle src_palette, INTEGER entry_index,
                               GUEST<INTEGER> *dst_usage,
                               GUEST<INTEGER> *dst_tolerance)
{
    ColorInfo *entry;

    entry = &PALETTE_INFO(src_palette)[entry_index];
    *dst_usage = entry->ciUsage;
    *dst_tolerance = entry->ciTolerance;
}

void Executor::C_SetEntryUsage(PaletteHandle dst_palette, INTEGER entry_index,
                               INTEGER src_usage, INTEGER src_tolerance)
{
    ColorInfo *entry;

    if(entry_index >= PALETTE_ENTRIES(dst_palette))
    {
        warning_unexpected("attempt to set entry beyond end of palette");
        return;
    }
    entry = &PALETTE_INFO(dst_palette)[entry_index];

    CI_USAGE(entry) = src_usage;
    CI_TOLERANCE(entry) = src_tolerance;

    pm_deallocate_entry(entry, false);

    PALETTE_SET_MODIFIED(dst_palette);
}

void Executor::C_CTab2Palette(CTabHandle src_ctab, PaletteHandle dst_palette,
                              INTEGER src_usage, INTEGER src_tolerance)
{
    int ctab_size;
    ColorSpec *ctab_table;
    ColorInfo *palette_info;
    int i;

    if(!src_ctab
       || !dst_palette)
        return;

    /* this isn't quite right; passing in a zero-sized handle for the
     palette made the mac do nothing */

    ctab_size = CTAB_SIZE(src_ctab);
    /* resize the palette */
    SetHandleSize((Handle)dst_palette,
                  PALETTE_STORAGE_FOR_ENTRIES(ctab_size + 1));
    PALETTE_ENTRIES(dst_palette) = ctab_size + 1;

    ctab_table = CTAB_TABLE(src_ctab);
    palette_info = PALETTE_INFO(dst_palette);
    for(i = 0; i <= ctab_size; i++)
    {
        ColorInfo *entry;

        entry = &palette_info[i];
        pm_deallocate_entry(entry, false);

        entry->ciRGB = ctab_table[i].rgb;
        entry->ciUsage = src_usage;
        entry->ciTolerance = src_tolerance;
    }
    PALETTE_SET_MODIFIED(dst_palette);
}

void Executor::C_Palette2CTab(PaletteHandle src_palette, CTabHandle dst_ctab)
{
    int palette_entries;
    ColorInfo *palette_info;
    ColorSpec *ctab_table;
    int i;

    if(!src_palette
       || !dst_ctab)
        return;

    palette_entries = PALETTE_ENTRIES(src_palette);

    SetHandleSize((Handle)dst_ctab,
                  CTAB_STORAGE_FOR_SIZE(palette_entries - 1));

    CTAB_SEED(dst_ctab) = 0;
    CTAB_FLAGS(dst_ctab) = 0;
    CTAB_SIZE(dst_ctab) = palette_entries - 1;

    palette_info = PALETTE_INFO(src_palette);
    ctab_table = CTAB_TABLE(dst_ctab);
    for(i = 0; i < palette_entries; i++)
    {
        ctab_table[i].value = i;

        ctab_table[i].rgb = palette_info[i].ciRGB;
    }
}

LONGINT Executor::C_Entry2Index(INTEGER entry_index)
{
    PaletteHandle palette;
    ColorInfo *entry;

    palette = get_current_palette();
    if(!palette)
        palette = GetPalette((WindowPtr)-1);

    if(entry_index > PALETTE_ENTRIES(palette))
        /* not sure what to do in the error case here */
        gui_abort();
    entry = &PALETTE_INFO(palette)[entry_index];

    if((CI_USAGE(entry) & pmTolerant)
       || CI_USAGE(entry) & pmCourteous)
    {
        /* return the index for ciRGB */
        return Color2Index(&entry->ciRGB);
    }
    else if(CI_USAGE(entry) & pmExplicit)
    {
        int gd_index_mask;

        gd_index_mask = CTAB_SIZE(PIXMAP_TABLE(GD_PMAP(LM(TheGDevice))));
        return ((long)entry_index) & gd_index_mask;
    }
    else if(CI_ALLOCATED_ENTRY_P(entry))
    {
        return CI_ENTRY_INDEX(entry);
    }
    else
        gui_fatal("unhandled entry usage `%d'", entry->ciUsage);
}

void Executor::C_CopyPalette(PaletteHandle src_palette,
                             PaletteHandle dst_palette, int16_t src_start,
                             int16_t dst_start, int16_t n_entries)
{
    int src_n_entries, dst_n_entries;
    ColorInfo *src_info, *dst_info;
    ColorInfo *dst_entry;
    int i;

    if(src_palette == nullptr || dst_palette == nullptr)
        return;

    src_n_entries = PALETTE_ENTRIES(src_palette);
    dst_n_entries = PALETTE_ENTRIES(dst_palette);

    if(src_n_entries < src_start)
        return;
    else if(src_n_entries < src_start + n_entries)
        n_entries = src_start - src_n_entries;

    if(dst_n_entries < dst_start + n_entries)
        ResizePalette(dst_palette, dst_start + n_entries);

    src_info = PALETTE_INFO(src_palette);
    dst_info = PALETTE_INFO(dst_palette);

    dst_entry = &dst_info[dst_start];

    memcpy(dst_entry, &src_info[src_start],
           n_entries * sizeof *dst_entry);

    for(i = 0; i < n_entries; i++, dst_entry++)
    {
        dst_entry->ciFlags = 0;
        dst_entry->ciPrivate = 0;
    }
}
