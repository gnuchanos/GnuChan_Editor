/*
 * ***** BEGIN GPL LICENSE BLOCK *****
 *
 * This program is free software; you can redistribute it and/or
 * modify it under the terms of the GNU General Public License
 * as published by the Free Software Foundation; either version 2
 * of the License, or (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software Foundation,
 * Inc., 51 Franklin Street, Fifth Floor, Boston, MA 02110-1301, USA.
 *
 * Contributor(s): Blender Foundation
 *
 * ***** END GPL LICENSE BLOCK *****
 *
 */

/** \file blender/blenloader/intern/versioning_range.c
 *  \ingroup blenloader
 */

#include "BLI_compiler_attrs.h"
#include "BLI_listbase.h"
#include "BLI_utildefines.h"
#include "BLI_math.h"

#include <stdio.h>
#include <string.h>

/* allow readfile to use deprecated functionality */
#define DNA_DEPRECATED_ALLOW

#include "DNA_camera_types.h"
#include "DNA_genfile.h"
#include "DNA_material_types.h"
#include "DNA_mesh_types.h"
#include "DNA_object_force_types.h"
#include "DNA_object_types.h"
#include "DNA_python_component_types.h"
#include "DNA_screen_types.h"
#include "DNA_sdna_types.h"
#include "DNA_sensor_types.h"
#include "DNA_space_types.h"
#include "DNA_view3d_types.h"
#include "DNA_world_types.h"

#include "BKE_main.h"
#include "BKE_node.h"
#include "BKE_screen.h"

#include "BLI_math_base.h"

#include "BLO_readfile.h"

#include "wm_event_types.h"

#include "readfile.h"

#include "MEM_guardedalloc.h"

static ARegion *do_versions_find_region_or_null(ListBase *regionbase, int regiontype)
{
  LISTBASE_FOREACH (ARegion *, region, regionbase) {
    if (region->regiontype == regiontype) {
      return region;
    }
  }
  return NULL;
}

void blo_do_versions_range(FileData *fd, Library *lib, Main *main)
{
  // printf("Range: open file from version : %i, subversion : %i\n", main->rangeversionfile,
  // main->rangesubversionfile);
  if (!MAIN_VERSION_RANGE_ATLEAST(main, 1, 0, 0)) {
    printf("Upbge/bge file is now updated for Range Game Engine.\n");
  }

  if (!MAIN_VERSION_RANGE_ATLEAST(main, 1, 2, 0)) {
    if (!DNA_struct_elem_find(fd->filesdna, "PythonComponent", "int", "flag_toggle_exec")) {

      for (Object *ob = main->object.first; ob; ob = ob->id.next) {
        for (PythonComponent *pc = ob->components.first; pc != NULL; pc = (PythonComponent *)pc->next) {
          pc->flag_toggle_exec = true;
        }
      }
    }
  }

  if (!MAIN_VERSION_RANGE_ATLEAST(main, 1, 4, 2)) {
    LISTBASE_FOREACH (bScreen *, screen, &main->screen) {
      LISTBASE_FOREACH (ScrArea *, sa, &screen->areabase) {
        LISTBASE_FOREACH (SpaceLink *, sl, &sa->spacedata) {
          ListBase *regionbase = (sl == sa->spacedata.first) ? &sa->regionbase : &sl->regionbase;
          ARegion *region_header = do_versions_find_region_or_null(regionbase, RGN_TYPE_HEADER);

          // Leave the header at the top.
          if (region_header) {
            region_header->alignment = RGN_ALIGN_TOP;
          }

          // If it is Space_Buts, we add the vertical bar.
          if (sl->spacetype == SPACE_BUTS) {
            /* we need to check if the navBar region was previously added by previews without
             * minsubversion */
            ARegion *region_navbar = do_versions_find_region_or_null(regionbase, RGN_TYPE_NAV_BAR);
            if (region_navbar) {
              continue;
            }

            /* if you got here, add */
            ARegion *ar = MEM_callocN(sizeof(ARegion), "navigation bar for properties");
            ARegion *ar_header = NULL;

            for (ar_header = regionbase->first; ar_header; ar_header = ar_header->next) {
              if (ar_header->regiontype == RGN_TYPE_HEADER) {
                break;
              }
            }
            BLI_assert(ar_header);

            BLI_insertlinkafter(regionbase, ar_header, ar);

            ar->regiontype = RGN_TYPE_NAV_BAR;
            ar->alignment = RGN_ALIGN_LEFT;
          }
        }
      }
    }
  }

  if (!MAIN_VERSION_RANGE_ATLEAST(main, 1, 4, 3)) {

    LISTBASE_FOREACH (bScreen *, screen, &main->screen) {
      LISTBASE_FOREACH (ScrArea *, area, &screen->areabase) {
        LISTBASE_FOREACH (SpaceLink *, slink, &area->spacedata) {
          if (slink->spacetype == SPACE_USERPREF) {
            ARegion *navigation_region = BKE_spacedata_find_region_type(
                slink, area, RGN_TYPE_NAV_BAR);

            if (!navigation_region) {
              ListBase *regionbase = (slink == area->spacedata.first) ? &area->regionbase :
                                                                        &slink->regionbase;

              navigation_region = MEM_callocN(sizeof(ARegion),
                                              "userpref navigation-region do_versions");

              BLI_addhead(regionbase, navigation_region); /* order matters, addhead not addtail! */
              navigation_region->regiontype = RGN_TYPE_NAV_BAR;
              navigation_region->alignment = RGN_ALIGN_LEFT;
            }
          }
        }
      }
    }
  }
  if (!MAIN_VERSION_RANGE_ATLEAST(main, 1, 5, 1)) {
    LISTBASE_FOREACH (Scene *, scene, &main->scene) {
      scene->gm.cursor_size = 20;
      scene->orientation_index_custom = -1;
    }
    LISTBASE_FOREACH (bScreen *, sc, &main->screen) {
      LISTBASE_FOREACH (ScrArea *, sa, &sc->areabase) {
        LISTBASE_FOREACH (SpaceLink *, sl, &sa->spacedata) {
          if (sl->spacetype == SPACE_VIEW3D) {
            View3D *v3d = (View3D *)sl;
            v3d->flag2 |= V3D_RENDER_SHOW_COMPONENTS;
          }
        }
      }
    }

    LISTBASE_FOREACH (Object *, ob, &main->object) {
      ob->gameflag |= OB_TASK_CONVERT;
    }

    LISTBASE_FOREACH (World *, wo, &main->world) {
      wo->sun_size = 0.2f;
      wo->turbidity = 0.2f;
      wo->ground = 1.0f;
    }
  }

  if (!MAIN_VERSION_RANGE_ATLEAST(main, 1, 5, 100)) {
    LISTBASE_FOREACH (Material *, ma, &main->mat) {
      if (ma->ref == 0.8f) {
        ma->ref = 1.0;
      }
    }
    LISTBASE_FOREACH (World *, wo, &main->world) {
      if (wo->range == 0.8f) {
        wo->range = 1.0;
      }
    }
  }

  /* 1.5a */
  if (!MAIN_VERSION_RANGE_ATLEAST(main, 1, 5, 101)) {
    LISTBASE_FOREACH (World *, wo, &main->world) {
      if (wo->turbidity == 1.0f) {
        wo->sun_size = 0.1f;
        wo->turbidity = 0.5f;
        wo->ground = 0.85f;
      }
    }
  }

  /* 1.6 */
  if (!MAIN_VERSION_RANGE_ATLEAST(main, 1, 6, 100)) {
    LISTBASE_FOREACH (Lamp *, light, &main->lamp) {
      if (light->type == LA_SUN) {
        light->spotblend = 0.05f;
      }
    }

    // Change default IOR value
    LISTBASE_FOREACH (Material *, ma, &main->mat) {
      if (ma->refrac == 4.0f) {
        ma->refrac = 1.5f;
      }
    }

	// Change Font Name
    LISTBASE_FOREACH (VFont *, vf, &main->vfont) {
      if (strcmp(vf->id.name, "VFBfont") == 0) {
        strcpy(vf->id.name, "VFRoboto-Medium");
      }
    }

    LISTBASE_FOREACH (Object *, ob, &main->object) {
      LISTBASE_FOREACH (bSensor *, sens, &ob->sensors) {
        // Set default sensor color.
        if (U.themes.first) {
          bTheme *btheme = U.themes.first;
          copy_v4_v4_uchar(sens->color, btheme->tui.wcol_box.inner);
        }
      }
    }

    LISTBASE_FOREACH (World *, wo, &main->world) {
      wo->mistheight = 5.0f;
      wo->mistdensity = 1.0f;
    }

    LISTBASE_FOREACH (Scene *, scene, &main->scene) {
      // Old FXAA location
      if (scene->gm.aasamples == 1) {
        scene->gm.aasamples = 0;
        scene->scenefx_settings.scenefx_flag |= SCENE_FX_FLAG_FXAA;
      }
    }
  }

  /* 1.6 rev1 */
  if (!MAIN_VERSION_RANGE_ATLEAST(main, 1, 6, 1001)) {
    LISTBASE_FOREACH (Scene *, scene, &main->scene) {
      // SSAO
      scene->scenefx_settings.ssao_lod = 2;

      // Saturation and Temperature
      if (scene->scenefx_settings.tonemap) {
        scene->scenefx_settings.tonemap->saturation = 1.0f;
        scene->scenefx_settings.tonemap->temperature = 0.5f;
      }

	  // Light Scatter
	  if (scene->scenefx_settings.scatter) {
		scene->scenefx_settings.scatter->intensity = 0.5f;
		scene->scenefx_settings.scatter->threshold = 0.75f;
		scene->scenefx_settings.scatter->stepsize = 1.0f;
	  }
    }

	LISTBASE_FOREACH (Lamp *, lamp, &main->lamp) {
		// Cascaded Shadow Mapping
		lamp->csm_proportion_two = 0.1f;
		lamp->csm_proportion_three = 0.2f;
		lamp->bufsizetwo = 512;
		lamp->bufsizethree = 512;
		lamp->soft_two = 5.0f;
		lamp->bias_two = 1.0f;
		lamp->soft_three = 8.0f;
		lamp->bias_three = 1.0f;
    }

	LISTBASE_FOREACH (Camera *, camera, &main->camera) {
		// Disable Override Culling (Fake News from Uniday Studio)
		camera->gameflag &= ~GAME_CAM_OVERRIDE_CULLING;
    }
  }

  /* 1.6 rev2 */
  if (!MAIN_VERSION_RANGE_ATLEAST(main, 1, 6, 1002)) {
	  LISTBASE_FOREACH (Image *, ima, &main->image) {
		ima->flag |= IMA_MIPMAP | IMA_ANISOTROPIC_FILTER;
	}
  }
}
