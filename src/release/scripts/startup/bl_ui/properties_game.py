# ##### BEGIN GPL LICENSE BLOCK #####
#
#  This program is free software; you can redistribute it and/or
#  modify it under the terms of the GNU General Public License
#  as published by the Free Software Foundation; either version 2
#  of the License, or (at your option) any later version.
#
#  This program is distributed in the hope that it will be useful,
#  but WITHOUT ANY WARRANTY; without even the implied warranty of
#  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
#  GNU General Public License for more details.
#
#  You should have received a copy of the GNU General Public License
#  along with this program; if not, write to the Free Software Foundation,
#  Inc., 51 Franklin Street, Fifth Floor, Boston, MA 02110-1301, USA.
#
# ##### END GPL LICENSE BLOCK #####

# <pep8 compliant>
import bpy
from bpy.types import Panel, Menu, UIList, Operator, AnimationEventTrigger

class GameButtonsPanel:
    bl_space_type = 'PROPERTIES'
    bl_region_type = 'WINDOW'
    bl_context = "game"
    bl_order = 1000

class GAME_PT_game_components(GameButtonsPanel, Panel):
    bl_label = "Game Components"

    @classmethod
    def poll(cls, context):
        ob = context.active_object
        return ob and ob.game

    def draw(self, context):
        layout = self.layout

        ob = context.active_object
        game = ob.game

        row = layout.row()
        row.operator("logic.python_component_register", text="Add", icon="PLUS")
        row.operator("logic.python_component_create", text="Create", icon="PLUS")

        for i, c in enumerate(game.components):
            box = layout.box()
            row = box.row(align=1)
            row.prop(c, "show_expanded", text="", emboss=0)
            row.prop(c, "toggle_execution", text="", emboss=False)
            if "C_Icons" in c.properties:
                try:
                    icondict = c.properties["C_Icons"].value.split("+")
                    row.label(c.name, icon=icondict[0])
                except:
                    row.label(c.name)
            else:
                row.label(c.name)
            
            row.operator("logic.python_component_reload", icon="RECOVER_LAST", text="").index = i
            # row.separator()
            row.operator("logic.python_component_move_up", icon="TRIA_UP", text="").index = i
            row.operator("logic.python_component_move_down", icon="TRIA_DOWN", text="").index = i
            row = row.row(align=0)
            row.operator("logic.python_component_remove", text="", icon='X').index = i
            lastHeader = None

            if len(c.properties) == 1 and c.properties[0].name == "C_Icons": continue
            if c.show_expanded and len(c.properties) > 0:
                expanded  = True
                collapsed = True
                box = box.box().column()
                armature=""

                for prop in c.properties:
                    if prop.name[:8] == "C_Header":
                        row = box.row()
                        expanded  = prop.value
                        collapsed = prop.value
                        split=prop.name.split("/")
                        name=split[1] if len(split) > 1 else "HEADER"
                        icon=split[2] if len(split) > 2 else "FULLSCREEN"
                        row.scale_y = 1.2
                        try:
                            row.prop(prop,"value", text=name, toggle=1, icon=icon, emboss=1)
                        except:
                            print(f"ERROR! Please check header: '{prop.name}' It needs to be like this 'C_Header/HeaderName/Icon'")
                        continue

                    elif prop.name[:10] == "C_Collapse" and collapsed:
                        collapsed  = prop.value
                        split=prop.name.split("/")
                        name = " ".join(split[1]) if len(split) > 1 else "COLLAPSE"
                        icon = split[2] if len(split) > 2 else "MOD_BOOLEAN"

                        # LABEL
                        row = box.row() ; row = box.row() ; row = box.row()
                        row.prop(prop,"value",text=name,toggle=1,emboss=0 if collapsed else 1, icon=icon)
                        row.scale_y = 0.9

                        # SEPARATOR
                        row = box.row()
                        row.prop(prop,"value",text=" ",toggle=1,emboss=1)
                        row.scale_y = 0.2
                        row.enabled = False

                        if collapsed: row = box.row() ; row = box.row() ; row = box.row() ; row = box.row() ; row = box.row()
                        continue
                    else:
                        if expanded and collapsed: row = box.row()
                    text=prop.name

                    try:
                        if expanded and collapsed:
                            split = prop.name.split("/") if "@" in prop.name else [prop.name]
                            if len(split)>1: text=split[1]+":"

                            if not prop.name=="C_Icons": 
                                row.label(text=text, icon="DOT")
                                col = row.column()
                                col.prop(prop, "value", text="")
                    except:
                        if expanded and collapsed:
                            row.label(text=text)
                            col = row.column()
                            col.prop(prop, "value", text="")
                        
        layout.operator("wm.flowmenu_ot_init", text="Add...", icon="PLUS")

class GAME_PT_game_properties(GameButtonsPanel, Panel):
    bl_label = "Game Properties"

    @classmethod
    def poll(cls, context):
        ob = context.active_object
        return ob and ob.game

    def draw(self, context):
        layout = self.layout

        ob = context.active_object
        game = ob.game
        is_font = (ob.type == 'FONT')

        if is_font:
            prop_index, prop_indexR = game.properties.find("Text"), game.properties.find("Text-Res")

            if prop_index != -1:
                layout.operator("object.game_property_remove", text="Remove Text Game Property", icon='X').index = prop_index
                row = layout.row()
                sub = row.row()
                sub.enabled = 0
                prop = game.properties[prop_index]
                sub.prop(prop, "name", text="", icon="FONT_DATA")
                row.prop(prop, "type", text="")
                row.label("See Text Object")
                
                if prop_indexR != -1:
                    sub = row.row(align=True)
                    sub.operator("object.game_property_remove", text="", icon='X').index = prop_indexR
                    row = layout.row()
                    sub = row.row()
                    sub.enabled = 0
                    prop = game.properties[prop_indexR]
                    sub.prop(prop, "name", text="", icon="FONT_DATA")
                    row.prop(prop, "value", text="Resolution")
                    row.label("Text Resolution(0 - 50)")
                else:
                    sub = row.row(align=True)
                    propsR = sub.operator("object.game_property_new", text="", icon='ZOOMIN',)
                    propsR.name = "Text-Res"
                    propsR.type = "FLOAT"
            else:
                props = layout.operator("object.game_property_new", text="Add Text Game Property", icon='ZOOMIN')
                props.name = "Text"
                props.type = "STRING"

        props = layout.operator("object.game_property_new", text="Add Game Property", icon='PLUS')
        props.name = ""

        for i, prop in enumerate(game.properties):
            if is_font and i == prop_index or is_font and i == prop_indexR:
                continue

            box = layout.box()
            row = box.row()
            row.prop(prop, "name", text="")
            row.prop(prop, "type", text="")
            row.prop(prop, "value", text="")
            row.prop(prop, "show_debug", text="", toggle=True, icon='INFO')
            sub = row.row(align=True)
            props = sub.operator("object.game_property_move", text="", icon='TRIA_UP')
            props.index = i
            props.direction = 'UP'
            props = sub.operator("object.game_property_move", text="", icon='TRIA_DOWN')
            props.index = i
            props.direction = 'DOWN'
            row.operator("object.game_property_remove", text="", icon='X', emboss=False).index = i

class PhysicsButtonsPanel:
    bl_space_type = 'PROPERTIES'
    bl_region_type = 'WINDOW'
    bl_context = "physics"

class PHYSICS_PT_game_physics(PhysicsButtonsPanel, Panel):
    bl_label = "Physics"
    COMPAT_ENGINES = {'BLENDER_GAME'}

    @classmethod
    def poll(cls, context):
        ob = context.active_object
        rd = context.scene.render
        return ob and ob.game and (rd.engine in cls.COMPAT_ENGINES)

    def draw(self, context):
        layout = self.layout

        ob = context.active_object
        game = ob.game
        soft = ob.game.soft_body
        
        physics_type = game.physics_type
        iconType = "X"
        if physics_type == "CHARACTER": iconType = "POSE_HLT"
        elif physics_type == "DYNAMIC": iconType = "VIEW3D"
        elif physics_type == "STATIC": iconType = "VIEW3D"
        elif physics_type == "RIGID_BODY": iconType = "VIEW3D"
        elif physics_type == "SOFT_BODY": iconType = "SNAP_VOLUME"
        elif physics_type == "OCCLUDER": iconType = "RESTRICT_RENDER_ON"
        elif physics_type == "SENSOR": iconType = "RESTRICT_VIEW_OFF"
        elif physics_type == "NAVMESH": iconType = "GHOST_ENABLED"
        layout.prop(game, "physics_type", icon=iconType)
        layout.separator()

        if physics_type == 'CHARACTER':
            layout.prop(game, "use_actor")
            layout.prop(ob, "hide_render", text="Invisible")  # out of place but useful
            
            # layout.separator()
            layout.label(text="Character Attributes:", icon="OUTLINER_OB_ARMATURE")
            split = layout.split()
            
            col = split.column()
            col.prop(game, "step_height", slider=True)
            col.prop(game, "fall_speed")
            col.prop(game, "max_slope")
            col.prop(game, "smooth_movement")
            col = split.column()
            col.prop(game, "jump_speed")
            col.prop(game, "jump_max")
            col.prop(game, "radius")
            col.prop(game, "jump_direction")

        elif physics_type in {'DYNAMIC', 'RIGID_BODY'}:
            split = layout.split()

            col = split.column()
            col.prop(game, "use_actor")
            col.prop(game, "use_ghost")
            col.prop(ob, "hide_render", text="Invisible")  # out of place but useful

            col = split.column()
            col.prop(game, "use_physics_fh")
            col.prop(game, "use_rotate_from_normal")
            col.prop(game, "use_sleep")

            layout.separator()

            split = layout.split()

            col = split.column()
            if physics_type == "DYNAMIC":   col.label(text="Dynamic Attributes:", icon="VIEW3D")
            else: col.label(text="Rigid Body Attributes:", icon="VIEW3D")
            
            col.prop(game, "mass")
            col.prop(game, "radius")
            col.prop(game, "form_factor")
            col.prop(game, "elasticity", slider=1)

            col.label(text="Linear Velocity:", icon="FORCE_HARMONIC")
            sub = col.column(align=1)
            sub.prop(game, "velocity_min", text="Minimum")
            sub.prop(game, "velocity_max", text="Maximum")

            col = split.column()
            col.label(text="Friction:", icon="HAIR")
            col.prop(game, "friction")
            col.prop(game, "rolling_friction")
            col.separator()

            sub = col.column()
            sub.prop(game, "use_anisotropic_friction")
            subsub = sub.column()
            subsub.active = game.use_anisotropic_friction
            subsub.prop(game, "friction_coefficients", text="", slider=True)

            split = layout.split()
            col = split.column()
            col.label(text="Angular velocity:", icon="FORCE_MAGNETIC")
            sub = col.column(align=True)
            sub.prop(game, "angular_velocity_min", text="Minimum")
            sub.prop(game, "angular_velocity_max", text="Maximum")

            col = split.column()
            col.label(text="Damping:", icon="META_CUBE")
            sub = col.column(align=True)
            sub.prop(game, "damping", text="Translation", slider=True)
            sub.prop(game, "rotation_damping", text="Rotation", slider=True)

            layout.separator()

            col = layout.column()

            col.label(text="Lock Translation:", icon="LINKED")
            row = col.row()
            row.prop(game, "lock_location_x", text="X")
            row.prop(game, "lock_location_y", text="Y")
            row.prop(game, "lock_location_z", text="Z")

        if physics_type == 'RIGID_BODY':
            col = layout.column()

            col.label(text="Lock Rotation:", icon="LINKED")
            row = col.row()
            row.prop(game, "lock_rotation_x", text="X")
            row.prop(game, "lock_rotation_y", text="Y")
            row.prop(game, "lock_rotation_z", text="Z")

        elif physics_type == 'SOFT_BODY':
            col = layout.column()
            col.prop(game, "use_actor")
            col.prop(game, "use_ghost")
            col.prop(ob, "hide_render", text="Invisible")

            layout.separator()

            split = layout.split()

            col = split.column()
            col.label(text="General Attributes:", icon="SNAP_VOLUME")
            col.prop(game, "mass")
            # disabled in the code
            # col.prop(soft, "weld_threshold")
            col.prop(soft, "linear_stiffness", slider=True)
            col.prop(soft, "dynamic_friction", slider=True)
            col.prop(soft, "kdp", text="Damping", slider=True)
            col.prop(soft, "collision_margin", slider=True)
            col.prop(soft, "kvcf", text="Velocity Correction", slider=True)
            col.prop(soft, "use_bending_constraints", text="Bending Constraints")

            sub = col.column()
            sub.active = soft.use_bending_constraints
            sub.prop(soft, "bending_distance")

            col.prop(soft, "use_shape_match")

            sub = col.column()
            sub.active = soft.use_shape_match
            sub.prop(soft, "shape_threshold", slider=True)

            col.label(text="Solver Iterations:", icon="SNAP_FACE")
            col.prop(soft, "position_solver_iterations", text="Position Solver")
            col.prop(soft, "velocity_solver_iterations", text="Velocity Solver")
            col.prop(soft, "cluster_solver_iterations", text="Cluster Solver")
            col.prop(soft, "drift_solver_iterations", text="Drift Solver")

            col = split.column()
            col.label(text="Hardness:", icon="OUTLINER_OB_FORCE_FIELD")
            col.prop(soft, "kchr", text="Rigid Contacts", slider=True)
            col.prop(soft, "kkhr", text="Kinetic Contacts", slider=True)
            col.prop(soft, "kshr", text="Soft Contacts", slider=True)
            col.prop(soft, "kahr", text="Anchors", slider=True)

            col.label(text="Cluster Collision:", icon="SNAP_VOLUME")
            col.prop(soft, "use_cluster_rigid_to_softbody")
            col.prop(soft, "use_cluster_soft_to_softbody")
            sub = col.column()
            sub.active = (soft.use_cluster_rigid_to_softbody or soft.use_cluster_soft_to_softbody)
            sub.prop(soft, "cluster_iterations", text="Iterations")
            sub.prop(soft, "ksrhr_cl", text="Rigid Hardness", slider=True)
            sub.prop(soft, "kskhr_cl", text="Kinetic Hardness", slider=True)
            sub.prop(soft, "ksshr_cl", text="Soft Hardness", slider=True)
            sub.prop(soft, "ksr_split_cl", text="Rigid Impulse Split", slider=True)
            sub.prop(soft, "ksk_split_cl", text="Kinetic Impulse Split", slider=True)
            sub.prop(soft, "kss_split_cl", text="Soft Impulse Split", slider=True)

            split = layout.split()

            col = split.column()
            col.label(text="Volume:", icon="META_BALL")
            col.prop(soft, "kpr", text="Pressure Coefficient")
            col.prop(soft, "kvc", text="Volume Conservation")

            col = split.column()
            col.label(text="Aerodynamics:", icon="FORCE_DRAG")
            col.prop(soft, "kdg", text="Drag Coefficient")
            col.prop(soft, "klf", text="Lift Coefficient")

        elif physics_type == 'STATIC':
            col = layout.column()
            col.prop(game, "use_actor")
            col.prop(game, "use_ghost")
            col.prop(ob, "hide_render", text="Invisible")

            layout.separator()

            split = layout.split()

            col = split.column()
            col.label(text="Static Attributes:", icon="VIEW3D")
            col.prop(game, "radius")
            col.prop(game, "elasticity", slider=True)
            col.label(text="Friction:", icon="HAIR")
            col.prop(game, "friction")
            col.prop(game, "rolling_friction")

            col = split.column()
            sub = col.column()
            sub.prop(game, "use_anisotropic_friction")
            subsub = sub.column()
            subsub.active = game.use_anisotropic_friction
            subsub.prop(game, "friction_coefficients", text="", slider=True)

        elif physics_type == 'SENSOR':
            col = layout.column()
            col.prop(game, "use_actor", text="Detect Actors")
            col.label(text="Sensor Attributes:")
            col.prop(game, "radius")
            col.prop(ob, "hide_render", text="Invisible")

        elif physics_type in {'INVISIBLE', 'NO_COLLISION', 'OCCLUDER'}:
            layout.prop(ob, "hide_render", text="Invisible")

        elif physics_type == 'NAVMESH':
            layout.operator("mesh.navmesh_face_copy")
            layout.operator("mesh.navmesh_face_add")

            layout.separator()

            layout.operator("mesh.navmesh_reset")
            layout.operator("mesh.navmesh_clear")

        if physics_type in {"STATIC", "DYNAMIC", "RIGID_BODY"}:
            row = layout.row()
            row.label(text="Force Field:", icon="FORCE_FORCE")

            row = layout.row()
            row.prop(game, "fh_force")
            row.prop(game, "fh_damping", slider=True)

            row = layout.row()
            row.prop(game, "fh_distance")
            row.prop(game, "use_fh_normal")


class PHYSICS_PT_game_collision_bounds(PhysicsButtonsPanel, Panel):
    bl_label = "Collision Bounds"
    COMPAT_ENGINES = {'BLENDER_GAME'}

    @classmethod
    def poll(cls, context):
        game = context.object.game
        rd = context.scene.render
        return (rd.engine in cls.COMPAT_ENGINES) \
            and (game.physics_type in {'SENSOR', 'STATIC', 'DYNAMIC', 'RIGID_BODY', 'CHARACTER', 'SOFT_BODY'})

    def draw_header(self, context):
        game = context.active_object.game

        self.layout.prop(game, "use_collision_bounds", text="")

    def draw(self, context):
        layout = self.layout

        game = context.active_object.game
        split = layout.split()
        split.active = game.use_collision_bounds

        col = split.column()
        col.prop(game, "collision_bounds_type", text="Bounds")
        if (game.collision_bounds_type == "TRIANGLE_MESH"):
            col.prop(game, "collision_bound")

        row = col.row()
        row.prop(game, "collision_margin", text="Margin", slider=1)

        sub = row.row()
        sub.active = game.physics_type not in {'SOFT_BODY', 'CHARACTER'}
        sub.prop(game, "use_collision_compound", text="Children Compound")

        layout.separator()
        split = layout.split()
        col = split.column()
        col.prop(game, "collision_group")
        col = split.column()
        col.prop(game, "collision_mask")


class PHYSICS_PT_game_obstacles(PhysicsButtonsPanel, Panel):
    bl_label = "Create Obstacle"
    COMPAT_ENGINES = {'BLENDER_GAME'}

    @classmethod
    def poll(cls, context):
        game = context.object.game
        rd = context.scene.render
        return (rd.engine in cls.COMPAT_ENGINES) \
            and (game.physics_type in {'SENSOR', 'STATIC', 'DYNAMIC', 'RIGID_BODY', 'SOFT_BODY', 'CHARACTER', 'NO_COLLISION'})

    def draw_header(self, context):
        game = context.active_object.game

        self.layout.prop(game, "use_obstacle_create", text="")

    def draw(self, context):
        layout = self.layout

        game = context.active_object.game

        layout.active = game.use_obstacle_create

        row = layout.row()
        row.prop(game, "obstacle_radius", text="Radius")
        row.label()


class RenderButtonsPanel:
    bl_space_type = 'PROPERTIES'
    bl_region_type = 'WINDOW'
    bl_context = "render"

    @classmethod
    def poll(cls, context):
        rd = context.scene.render
        return (rd.engine in cls.COMPAT_ENGINES)

class RENDER_PT_embedded(RenderButtonsPanel, Panel):
    bl_label = "Embedded Player"
    COMPAT_ENGINES = {'BLENDER_GAME'}

    def draw(self, context):
        layout = self.layout

        rd = context.scene.render

        row = layout.row()
        row.operator("view3d.game_start", text="Start")
        row = layout.row()
        row.label(text="Resolution:", icon="SCENE")
        row = layout.row(align=True)
        row.prop(rd, "resolution_x", slider=False, text="X")
        row.prop(rd, "resolution_y", slider=False, text="Y")


class RENDER_PT_game_player(RenderButtonsPanel, Panel):
    bl_label = "Standalone Player"
    COMPAT_ENGINES = {'BLENDER_GAME'}

    def draw(self, context):
        import sys
        layout = self.layout
        not_osx = sys.platform != "darwin"

        gs = context.scene.game_settings

        row = layout.row()
        row.operator("wm.blenderplayer_start", text="Start")
        row = layout.row()
        row.label(text="Resolution:", icon="SCENE")
        row = layout.row(align=True)
        row.active = not_osx or not gs.show_fullscreen
        row.prop(gs, "resolution_x", slider=False, text="X")
        row.prop(gs, "resolution_y", slider=False, text="Y")
        row = layout.row()
        col = row.column()
        col.active = not gs.borderless_window
        col.prop(gs, "show_fullscreen")
        
        col = layout.column()
        col.prop(gs, "borderless_window")

        if not_osx:
            col = row.column()
            col.prop(gs, "use_desktop")
            col.active = gs.show_fullscreen and not gs.borderless_window

        col = layout.column()
        col.label(text="Quality:")
        col = layout.column(align=True)
        col.prop(gs, "depth", text="Bit Depth", slider=False)
        col.prop(gs, "frequency", text="Refresh Rate", slider=False)


class RENDER_PT_game_stereo(RenderButtonsPanel, Panel):
    bl_label = "Stereo"
    COMPAT_ENGINES = {'BLENDER_GAME'}

    def draw(self, context):
        layout = self.layout

        gs = context.scene.game_settings
        stereo_mode = gs.stereo

        # stereo options:
        layout.row().prop(gs, "stereo", expand=True)

        # stereo:
        if stereo_mode == 'STEREO':
            layout.prop(gs, "stereo_mode")
            layout.prop(gs, "stereo_eye_separation")


class RENDER_PT_game_shading(RenderButtonsPanel, Panel):
    bl_label = "Shading"
    COMPAT_ENGINES = {'BLENDER_GAME'}

    def draw(self, context):
        layout = self.layout

        gs = context.scene.game_settings

        split = layout.split()

        col = split.column()
        col.prop(gs, "use_glsl_lights", text="Lights", icon="LAMP_SPOT")
        col.prop(gs, "use_glsl_shaders", text="Shaders", icon="LAMP_AREA")
        col.prop(gs, "use_glsl_shadows", text="Shadows", icon="SNAP_FACE")
        col.prop(gs, "use_glsl_environment_lighting", text="Environment Lighting", icon="SNAP_VOLUME")
        col = split.column()
        col.prop(gs, "use_glsl_ramps", text="Ramps", icon="IPO_BEZIER")
        col.prop(gs, "use_glsl_nodes", text="Nodes", icon="NODETREE")
        col.prop(gs, "use_glsl_extra_textures", text="Extra Textures", icon="ASSET_MANAGER")
        
class RENDER_PT_game_post_process_shaders(RenderButtonsPanel, Panel):
    bl_label = "Post Processing Shaders"
    COMPAT_ENGINES = {'BLENDER_GAME'}

    def draw(self, context):
        layout = self.layout
        
        scene = context.scene
        scenefx_settings = scene.scenefx_settings
        
        row = layout.row(align=True)
        row.prop(scenefx_settings, "show_expanded_ssao", text="Ambient Occlusion", emboss=True)
        row.prop(scenefx_settings, "render_editor_ssao", text="", icon="RESTRICT_RENDER_OFF", emboss=True)
        row.prop(scenefx_settings, "use_ssao", text="")
        
        if (scenefx_settings.show_expanded_ssao and scenefx_settings.ssao):
            ssao_settings = scenefx_settings.ssao
            subcol = layout.column(align=True)
            subcol.active = scenefx_settings.use_ssao
            subcol.prop(scenefx_settings, "ssao_lod", text="Lod")
            subcol.prop(ssao_settings, "factor")
            subcol.prop(ssao_settings, "distance_max")
            subcol.prop(ssao_settings, "attenuation")
            subcol.prop(ssao_settings, "samples")
            
            subcol_gi = layout.column(align=True)
            subcol_gi.active = scenefx_settings.use_ssao_gi
            subcol_gi.prop(scenefx_settings, "use_ssao_gi")
            subcol_gi.prop(ssao_settings, "gi_irradiance")
            
        row = layout.row(align=True)
        row.prop(scenefx_settings, "show_expanded_bloom", text="Bloom", emboss=True)
        row.prop(scenefx_settings, "render_editor_bloom", text="", icon="RESTRICT_RENDER_OFF", emboss=True)
        row.prop(scenefx_settings, "use_bloom", text="")
            
        if (scenefx_settings.show_expanded_bloom and scenefx_settings.bloom):
            bloom_settings = scenefx_settings.bloom
            subcol = layout.column(align=True)
            subcol.active = scenefx_settings.use_bloom
            subcol.prop(bloom_settings, "intensity")
            subcol.prop(bloom_settings, "threshold")
            
        row = layout.row(align=True)
        row.prop(scenefx_settings, "show_expanded_vignette", text="Vignette", emboss=True)
        row.prop(scenefx_settings, "render_editor_vignette", text="", icon="RESTRICT_RENDER_OFF", emboss=True)
        row.prop(scenefx_settings, "use_vignette", text="")
            
        if (scenefx_settings.show_expanded_vignette and scenefx_settings.vignette):
            vignette_settings = scenefx_settings.vignette
            subcol = layout.column(align=True)
            subcol.active = scenefx_settings.use_vignette
            subcol.prop(vignette_settings, "size")
            subcol.prop(vignette_settings, "radius")
            
        row = layout.row(align=True)
        row.prop(scenefx_settings, "show_expanded_tonemap", text="Tonemap", emboss=True)
        row.prop(scenefx_settings, "render_editor_tonemap", text="", icon="RESTRICT_RENDER_OFF", emboss=True)
        row.prop(scenefx_settings, "use_tonemap", text="")
            
        if (scenefx_settings.show_expanded_tonemap and scenefx_settings.tonemap):
            tonemap_settings = scenefx_settings.tonemap
            subcol = layout.column(align=True)
            subcol.active = scenefx_settings.use_tonemap
            subcol.prop(tonemap_settings, "shadertype")
            subcol.prop(tonemap_settings, "exposure")
            subcol.prop(tonemap_settings, "gamma")
            subcol.prop(tonemap_settings, "saturation")
            subcol.prop(tonemap_settings, "temperature")
            
        row = layout.row(align=True)
        row.prop(scenefx_settings, "show_expanded_lightscatter", text="Light Scattering", emboss=True)
        row.prop(scenefx_settings, "render_editor_lightscatter", text="", icon="RESTRICT_RENDER_OFF", emboss=True)
        row.prop(scenefx_settings, "use_lightscatter", text="")
        
        if (scenefx_settings.show_expanded_lightscatter and scenefx_settings.scatter):
            scatter_settings = scenefx_settings.scatter
            subcol = layout.column(align=True)
            subcol.active = scenefx_settings.use_lightscatter
            subcol.prop(scenefx_settings, "scatter_lod", text="Lod")
            subcol.prop(scatter_settings, "intensity")
            subcol.prop(scatter_settings, "threshold")
            subcol.prop(scatter_settings, "stepsize")
            subcol.prop(scatter_settings, "stepmax")
            
        row = layout.row(align=True)
        row.prop(scenefx_settings, "show_expanded_ssr", text="Screen Space Reflections", emboss=True)
        row.prop(scenefx_settings, "render_editor_ssr", text="", icon="RESTRICT_RENDER_OFF", emboss=True)
        row.prop(scenefx_settings, "use_ssr", text="")
        
        if (scenefx_settings.show_expanded_ssr and scenefx_settings.ssr):
            ssr_settings = scenefx_settings.ssr
            subcol = layout.column(align=True)
            subcol.active = scenefx_settings.use_ssr
            subcol.prop(scenefx_settings, "ssr_lod", text="Lod")
            subcol.prop(ssr_settings, "step_max")
            subcol.prop(ssr_settings, "roughness")
            subcol.prop(ssr_settings, "bias")
            subcol.prop(ssr_settings, "max_distance")
            
        row = layout.row(align=True)
        row.label("FXAA")
        row.prop(scenefx_settings, "render_editor_fxaa", text="", icon="RESTRICT_RENDER_OFF", emboss=True)
        row.prop(scenefx_settings, "use_fxaa", text="")


class RENDER_PT_game_system(RenderButtonsPanel, Panel):
    bl_label = "System"
    COMPAT_ENGINES = {'BLENDER_GAME'}

    def draw(self, context):
        layout = self.layout

        gs = context.scene.game_settings

        split = layout.split(factor=0.4)
        split.prop(gs, "use_frame_rate")
        split.prop(gs, "use_deprecation_warnings")
        
        col = layout.column()
        col.prop(gs, "vsync", icon="RENDER_STILL")
        col.prop(gs, "samples", icon="RENDER_STILL")
        col.prop(gs, "hdr", icon="RENDER_STILL")

        row = layout.row()
        col = row.column()
        
        col.label("Game Exit Key:", icon="BLENDER")
        
        col.active = not gs.ignore_exit_key
        col.prop(gs, "exit_key", text="", event=True) 

        col = layout.column()
        col.use_property_split = True
        col.use_property_decorate = False
        
        if (gs.ignore_exit_key):
            box = col.box()
            box.label("It will not be possible to close the game by exit key, exitGame() event only!", icon="ERROR")
        col.prop(gs, "ignore_exit_key")

class RENDER_UL_attachments(UIList):
    def draw_item(self, context, layout, data, item, icon, active_data, active_propname, index):
        if item is not None:
            layout.prop(item, "name", text="", emboss=False, icon="TEXTURE")
            layout.label(text=str(index))
        else:
            layout.label(text="", icon="TEXTURE")

class RENDER_PT_game_attachments(RenderButtonsPanel, Panel):
    bl_label = "Attachments"
    COMPAT_ENGINES = {'BLENDER_GAME'}

    def draw(self, context):
        layout = self.layout

        gs = context.scene.game_settings

        row = layout.row()

        row.template_list("RENDER_UL_attachments", "", gs, "attachment_slots", gs, "active_attachment_index", rows=2)

        col = row.column(align=True)
        col.operator("scene.render_attachment_new", icon='ZOOMIN', text="")
        col.operator("scene.render_attachment_remove", icon='ZOOMOUT', text="")

        attachment = gs.active_attachment

        if attachment is not None:
            row = layout.row()
            row.prop(attachment, "type")
            row.prop(attachment, "hdr")

            if attachment.type == "CUSTOM":
                row = layout.row()
                row.prop(attachment, "size")


class RENDER_PT_game_animations(RenderButtonsPanel, Panel):
    bl_label = "Animations"
    COMPAT_ENGINES = {'BLENDER_GAME'}

    def draw(self, context):
        layout = self.layout

        gs = context.scene.game_settings

        layout.prop(context.scene.render, "fps", text="Animation Frame Rate", slider=False)
        layout.prop(gs, "use_restrict_animation_updates")


class RENDER_PT_game_display(RenderButtonsPanel, Panel):
    bl_label = "Display"
    COMPAT_ENGINES = {"BLENDER_GAME"}

    def draw(self, context):
        layout = self.layout

        gs = context.scene.game_settings

        col = layout.column()
        col.prop(gs, "show_mouse", text="Mouse Cursor")
        
        col.label(text="Custom Mouse Cursor:", icon="RESTRICT_SELECT_OFF")
        col.prop(gs, "cursor_filepath", text="")
        col.prop(gs, "cursor_size")
        col = layout.row()
        col.prop(gs, "cursor_offset_x")
        col.prop(gs, "cursor_offset_y")
        col.prop(gs, "cursor_mipmap")

        col = layout.column()
        col.label(text="Framing:", icon="IMAGE_COL")
        col.row().prop(gs, "frame_type", expand=True)
        col.prop(gs, "frame_color", text="")


class RENDER_PT_game_debug(RenderButtonsPanel, Panel):
    bl_label = "Debug"
    COMPAT_ENGINES = {'BLENDER_GAME'}

    def draw(self, context):
        layout = self.layout

        gs = context.scene.game_settings

        split = layout.split(factor=0.4)

        col = split.column()
        col.prop(gs, "show_framerate_profile", text="Framerate and Profile")
        col.prop(gs, "show_render_queries", text="Render Queries")
        col.prop(gs, "show_debug_properties", text="Properties")
        col.prop(gs, "show_physics_visualization", text="Physics Visualization")

        col = split.column()
        col.prop(gs, "show_bounding_box", text="Bounding Box")
        col.prop(gs, "show_armatures", text="Armatures")
        col.prop(gs, "show_camera_frustum", text="Camera Frustum")
        col.prop(gs, "show_shadow_frustum", text="Shadow Frustum")


class SceneButtonsPanel:
    bl_space_type = 'PROPERTIES'
    bl_region_type = 'WINDOW'
    bl_context = "scene"


class SCENE_PT_game_physics(SceneButtonsPanel, Panel):
    bl_label = "Physics"
    COMPAT_ENGINES = {'BLENDER_GAME'}

    @classmethod
    def poll(cls, context):
        scene = context.scene
        return (scene.render.engine in cls.COMPAT_ENGINES)

    def draw(self, context):
        layout = self.layout

        gs = context.scene.game_settings

        layout.prop(gs, "physics_engine", text="Engine")
        if gs.physics_engine != 'NONE':
            layout.prop(gs, "physics_solver", icon="PHYSICS")
            layout.prop(gs, "physics_gravity", text="Gravity")

            split = layout.split()

            col = split.column()
            col.label(text="Physics Steps:")
            sub = col.column(align=True)
            sub.prop(gs, "physics_step_max", text="Max")
            sub.prop(gs, "physics_step_sub", text="Substeps")

            col = split.column()
            col.label(text="Logic Steps:")
            col.prop(gs, "logic_step_max", text="Max")

            row = layout.row()
            row.prop(gs, "fps", text="FPS")
            row.prop(gs, "time_scale")

            col = layout.column()
            col.label(text="Physics Deactivation:")
            sub = col.row(align=True)
            sub.prop(gs, "deactivation_linear_threshold", text="Linear Threshold")
            sub.prop(gs, "deactivation_angular_threshold", text="Angular Threshold")
            sub = col.row()
            sub.prop(gs, "deactivation_time", text="Time")

            split = layout.split()

            col = split.column()
            col.label(text="Culling:")
            col.prop(gs, "use_occlusion_culling", text="Occlusion Culling")
            sub = col.column()
            sub.active = gs.use_occlusion_culling
            sub.prop(gs, "occlusion_culling_resolution", text="Resolution")

            col = split.column()
            col.label(text="Object Activity:")
            col.prop(gs, "use_activity_culling")

        else:
            split = layout.split()

            col = split.column()
            col.label(text="Physics Steps:")
            col.prop(gs, "fps", text="FPS")

            col = split.column()
            col.label(text="Logic Steps:")
            col.prop(gs, "logic_step_max", text="Max")


class SCENE_PT_game_physics_obstacles(SceneButtonsPanel, Panel):
    bl_label = "Obstacle Simulation"
    bl_options = {'DEFAULT_CLOSED'}
    COMPAT_ENGINES = {'BLENDER_GAME'}

    @classmethod
    def poll(cls, context):
        scene = context.scene
        return (scene.render.engine in cls.COMPAT_ENGINES)

    def draw(self, context):
        layout = self.layout

        gs = context.scene.game_settings

        layout.prop(gs, "obstacle_simulation", text="Type")
        if gs.obstacle_simulation != 'NONE':
            layout.prop(gs, "level_height")
            layout.prop(gs, "show_obstacle_simulation")


class SCENE_PT_game_navmesh(SceneButtonsPanel, Panel):
    bl_label = "Navigation Mesh"
    bl_options = {'DEFAULT_CLOSED'}
    COMPAT_ENGINES = {'BLENDER_GAME'}

    @classmethod
    def poll(cls, context):
        scene = context.scene
        return (scene and scene.render.engine in cls.COMPAT_ENGINES)

    def draw(self, context):
        layout = self.layout

        rd = context.scene.game_settings.recast_data

        layout.operator("mesh.navmesh_make", text="Build Navigation Mesh")

        col = layout.column()
        col.label(text="Rasterization:")
        row = col.row()
        row.prop(rd, "cell_size")
        row.prop(rd, "cell_height")

        col = layout.column()
        col.label(text="Agent:")
        split = col.split()

        col = split.column()
        col.prop(rd, "agent_height", text="Height")
        col.prop(rd, "agent_radius", text="Radius")

        col = split.column()
        col.prop(rd, "slope_max")
        col.prop(rd, "climb_max")

        col = layout.column()
        col.label(text="Region:")
        row = col.row()
        row.prop(rd, "region_min_size")
        if rd.partitioning != 'LAYERS':
            row.prop(rd, "region_merge_size")

        col = layout.column()
        col.prop(rd, "partitioning")

        col = layout.column()
        col.label(text="Polygonization:")
        split = col.split()

        col = split.column()
        col.prop(rd, "edge_max_len")
        col.prop(rd, "edge_max_error")

        split.prop(rd, "verts_per_poly")

        col = layout.column()
        col.label(text="Detail Mesh:")
        row = col.row()
        row.prop(rd, "sample_dist")
        row.prop(rd, "sample_max_error")


class SCENE_PT_game_hysteresis(SceneButtonsPanel, Panel):
    bl_label = "Level of Detail"
    COMPAT_ENGINES = {'BLENDER_GAME'}

    @classmethod
    def poll(cls, context):
        scene = context.scene
        return (scene and scene.render.engine in cls.COMPAT_ENGINES)

    def draw(self, context):
        layout = self.layout
        gs = context.scene.game_settings

        row = layout.row()
        row.prop(gs, "use_scene_hysteresis", text="Hysteresis")
        row = layout.row()
        row.active = gs.use_scene_hysteresis
        row.prop(gs, "scene_hysteresis_percentage", text="")


class SCENE_PT_game_console(SceneButtonsPanel, Panel):
    bl_label = "Python Console"
    COMPAT_ENGINES = {'BLENDER_GAME'}

    @classmethod
    def poll(cls, context):
        scene = context.scene
        return (scene and scene.render.engine in cls.COMPAT_ENGINES)

    def draw_header(self, context):
        gs = context.scene.game_settings

        self.layout.prop(gs, "use_python_console", text="")

    def draw(self, context):
        layout = self.layout

        gs = context.scene.game_settings
        row = layout.row(align=True)
        row.active = gs.use_python_console
        row.label("Keys:")
        row.prop(gs, "python_console_key1", text="", event=True)
        row.prop(gs, "python_console_key2", text="", event=True)
        row.prop(gs, "python_console_key3", text="", event=True)
        row.prop(gs, "python_console_key4", text="", event=True)


class SCENE_PT_game_audio(SceneButtonsPanel, Panel):
    bl_label = "Audio"
    bl_options = {'DEFAULT_CLOSED'}
    COMPAT_ENGINES = {'BLENDER_GAME'}

    @classmethod
    def poll(cls, context):
        scene = context.scene
        return (scene and scene.render.engine in cls.COMPAT_ENGINES)

    def draw(self, context):
        layout = self.layout

        scene = context.scene

        split = layout.split()

        col = layout.column()
        col.prop(scene, "audio_distance_model", text="Distance Model")
        col = layout.column(align=True)
        col.prop(scene, "audio_doppler_speed", text="Speed")
        col.prop(scene, "audio_doppler_factor", text="Doppler")

        col.label("3D Audio:")
        col.prop(scene, "audio3d_update")


class WorldButtonsPanel:
    bl_space_type = 'PROPERTIES'
    bl_region_type = 'WINDOW'
    bl_context = "world"


class WORLD_PT_game_context_world(WorldButtonsPanel, Panel):
    bl_label = ""
    bl_options = {'HIDE_HEADER'}
    COMPAT_ENGINES = {'BLENDER_GAME'}

    @classmethod
    def poll(cls, context):
        rd = context.scene.render
        return (context.scene) and (rd.engine in cls.COMPAT_ENGINES)

    def draw(self, context):
        layout = self.layout

        scene = context.scene
        world = context.world
        space = context.space_data

        split = layout.split(factor=0.65)
        if scene:
            split.template_ID(scene, "world", new="world.new")
        elif world:
            split.template_ID(space, "pin_id")


class WORLD_PT_game_world(WorldButtonsPanel, Panel):
    bl_label = "World"
    COMPAT_ENGINES = {'BLENDER_GAME'}

    @classmethod
    def poll(cls, context):
        scene = context.scene
        return (scene.world and scene.render.engine in cls.COMPAT_ENGINES)

    def draw(self, context):
        layout = self.layout

        self.layout.template_preview(context.world)

        world = context.world
        
        row = layout.row()
        col = row.column()
        sub = col.column()
        sub.prop(world, "horizon_color", text="Horizon/Ambient Color")
        sub.prop(world, "ambient_color", text="")
        sub.separator(factor=0.4)
        
        if not world.use_sky_atmospheric:
            sub.prop(world, "zenith_color", text="Zenith/Nadir Color" if not world.use_sky_atmospheric else "Extinction/Inscattering Color")
            sub.prop(world, "nadir_color", text="")
            sub.active = world.use_sky_blend

        col = row.column()
        sub = col.column()
        row = layout.row()
        sub.prop(world, "use_sky_paper")
        sub.prop(world, "use_sky_blend")
        sub.prop(world, "use_sky_real")
        sub.prop(world, "use_sky_atmospheric")
        sub.prop(world, "use_sky_stars")
        sub.prop(world, "sun_size")
        
        if not world.use_sky_atmospheric:
            row.prop(world, "sky_turbidity")
            row.prop(world, "ground_color")


class WORLD_PT_game_environment_lighting(WorldButtonsPanel, Panel):
    bl_label = "Environment"
    COMPAT_ENGINES = {'BLENDER_GAME'}

    @classmethod
    def poll(cls, context):
        scene = context.scene
        return (scene.world and scene.render.engine in cls.COMPAT_ENGINES)

    def draw_header(self, context):
        light = context.world.light_settings
        self.layout.prop(light, "use_environment_light", text="")

    def draw(self, context):
        layout = self.layout

        light = context.world.light_settings
        world = context.world

        layout.active = light.use_environment_light

        split = layout.split()
        split.prop(light, "environment_energy", text="Light Energy")
        split.prop(light, "environment_color", text="")
        
        row = layout.row()
        row = layout.row()
        row.prop(world, "exposure")
        row.prop(world, "color_range", text="Color Range")


class WORLD_PT_game_mist(WorldButtonsPanel, Panel):
    bl_label = "Fog"
    COMPAT_ENGINES = {'BLENDER_GAME'}

    @classmethod
    def poll(cls, context):
        scene = context.scene
        return (scene.world and scene.render.engine in cls.COMPAT_ENGINES)

    def draw_header(self, context):
        world = context.world

        self.layout.prop(world.mist_settings, "use_mist", text="")

    def draw(self, context):
        layout = self.layout

        world = context.world

        layout.active = world.mist_settings.use_mist
        
        layout.prop(world, "fog_color", text="Fog Color")

        layout.prop(world.mist_settings, "falloff")
        layout.prop(world.mist_settings, "mist_blend_type")
        
        row = layout.row(align=True)
        
        row.prop(world.mist_settings, "start")
        row.prop(world.mist_settings, "depth")
        
        if world.mist_settings.falloff == 'HEIGHT':
            row = layout.row(align=True)
            row.prop(world.mist_settings, "height_fog")
            row.prop(world.mist_settings, "density_fog")

        layout.prop(world.mist_settings, "intensity", text="Minimum Intensity")


class DataButtonsPanel:
    bl_space_type = 'PROPERTIES'
    bl_region_type = 'WINDOW'
    bl_context = "data"


class DATA_PT_shadow_game(DataButtonsPanel, Panel):
    bl_label = "Use Shadow Mapping"
    COMPAT_ENGINES = {'BLENDER_GAME'}

    @classmethod
    def poll(cls, context):
        COMPAT_LIGHTS = {'SPOT', 'SUN'}
        lamp = context.lamp
        engine = context.scene.render.engine
        return (lamp and lamp.type in COMPAT_LIGHTS) and (engine in cls.COMPAT_ENGINES)

    def draw_header(self, context):
        lamp = context.lamp

        self.layout.prop(lamp, "use_shadow", text="")

    def draw(self, context):
        layout = self.layout

        lamp = context.lamp

        layout.active = lamp.use_shadow

        split = layout.split()

        col = split.column()
        col.prop(lamp, "shadow_color", text="")
        if lamp.type in ('SUN', 'SPOT'):
            col.prop(lamp, "show_shadow_box")
        col.prop(lamp, "static_shadow")

        col = split.column()
        if lamp.type == "SUN":
            col.prop(lamp, "shadow_blend", text="Shadow Fade", slider=True) # idk .. remove ?
        col.prop(lamp, "use_shadow_layer", text="This Layer Only")
        col.prop(lamp, "use_only_shadow")
        
        col = layout.column()
        
        col.label("Buffer Type:")
        col.prop(lamp, "ge_shadow_buffer_type", text="", toggle=True)
        if lamp.ge_shadow_buffer_type == "SIMPLE":
            col.label("Filter Type:")
            col.prop(lamp, "shadow_filter", text="", toggle=True)

        col.label("Shadow Quality:")
        col = layout.column(align=True)
        col.prop(lamp, "shadow_buffer_size", text="Size")
        
        if lamp.ge_shadow_buffer_type == "VARIANCE":
            col.prop(lamp, "shadow_buffer_sharp", text="Sharpness")
        elif lamp.shadow_filter in ("PCF", "PCF_BAIL", "PCF_JITTER", "PCF_PENUMBRA"):
            col.prop(lamp, "shadow_buffer_samples", text="Samples")
            col.prop(lamp, "shadow_buffer_soft", text="Soft")
            
        row = layout.row()
        row.label("Bias:")
        row = layout.row(align=True)
        row.prop(lamp, "shadow_buffer_bias", text="Bias")
        if lamp.ge_shadow_buffer_type == "VARIANCE":
            row.prop(lamp, "shadow_buffer_bleed_bias", text="Bleed Bias")
        else:
            row.prop(lamp, "shadow_buffer_slope_bias", text="Slope Bias")
        
        if lamp.type == "SUN":
            row = layout.row()
            row.label("Cascaded Shadow Mapping:")
            col = layout.column(align=True)
            col.active = lamp.use_csm_shadow
            col.prop(lamp, "use_csm_shadow", text="Enable Cascaded Shadow Mapping") # ToDO
            col.prop(lamp, "use_csm_debug", text="Enable Debug") # ToDO
            
            
            row = col.row(align=True)
            row.prop(lamp, "cascaded_proportion_two", text="Cascade Proportion Near", slider=True)
            row.prop(lamp, "cascaded_proportion_three", text="Cascade Proportion Middle", slider=True)
            
            col.separator(factor=1)
            
            col.prop(lamp, "shadow_csm_two_buffer_size", text="CSM Size Medium")
            col.prop(lamp, "shadow_buffer_soft_two", text="CSM Shadow Soft Medium")
            col.prop(lamp, "shadow_buffer_bias_two", text="CSM Bias Medium")
            
            col.separator(factor=1)
            
            col.prop(lamp, "shadow_csm_three_buffer_size", text="CSM Size Low")
            col.prop(lamp, "shadow_buffer_soft_three", text="CSM Shadow Soft Low")
            col.prop(lamp, "shadow_buffer_bias_three", text="CSM Bias Low")

        row = layout.row()
        row.label("Clipping:")
        row = layout.row(align=True)
        row.prop(lamp, "shadow_buffer_clip_start", text="Clip Start")
        row.prop(lamp, "shadow_buffer_clip_end", text="Clip End")

        if lamp.type == "SUN":
            row = layout.row()
            row.prop(lamp, "shadow_frustum_size", text="Frustum Size")


class ObjectButtonsPanel:
    bl_space_type = 'PROPERTIES'
    bl_region_type = 'WINDOW'
    bl_context = "object"


class OBJECT_MT_lod_tools(Menu):
    bl_label = "Level Of Detail Tools"

    def draw(self, context):
        layout = self.layout

        layout.operator("object.lod_by_name", text="Set By Name")
        layout.operator("object.lod_generate", text="Generate")
        layout.operator("object.lod_clear_all", text="Clear All", icon='PANEL_CLOSE')
        
class OBJECT_PT_game_object_tasks(ObjectButtonsPanel, Panel):
    bl_label = "Game Object Tasks"
    COMPAT_ENGINES = {'BLENDER_GAME'}

    @classmethod
    def poll(cls, context):
        ob = context.object
        return context.scene.render.engine in cls.COMPAT_ENGINES and ob.type

    def draw(self, context):
        layout = self.layout
        ob = context.object

        layout.prop(ob, "convert_object")
        
class OBJECT_MT_culling(ObjectButtonsPanel, Panel):
    bl_label = "Culling Bounding Volume"
    COMPAT_ENGINES = {'BLENDER_GAME'}

    @classmethod
    def poll(cls, context):
        ob = context.object
        return context.scene.render.engine in cls.COMPAT_ENGINES and ob.type not in {'CAMERA', 'EMPTY', 'LAMP'}

    def draw(self, context):
        layout = self.layout
        game = context.active_object.game

        layout.label(text="Predefined Bound:")
        layout.prop(game, "predefined_bound", "")

class OBJECT_PT_activity_culling(ObjectButtonsPanel, Panel):
    bl_label = "Activity Culling"
    COMPAT_ENGINES = {'BLENDER_GAME'}

    @classmethod
    def poll(cls, context):
        ob = context.object
        return context.scene.render.engine in cls.COMPAT_ENGINES and ob.type not in {'CAMERA'}

    def draw(self, context):
        layout = self.layout
        activity = context.object.game.activity_culling

        split = layout.split()

        col = split.column()
        col.prop(activity, "use_physics", text="Physics")
        sub = col.box()
        sub.active = activity.use_physics
        sub.prop(activity, "physics_radius")
        sub.prop(activity, "sleep_velocity", text="Sleep Velocity")

        col = split.column()
        col.prop(activity, "use_logic", text="Logic")
        sub = col.box()
        sub.active = activity.use_logic
        sub.prop(activity, "logic_radius")
        sub = sub.column()
        sub.prop(activity, "activity_components", text="No Components")

class OBJECT_PT_levels_of_detail(ObjectButtonsPanel, Panel):
    bl_label = "Levels of Detail"
    COMPAT_ENGINES = {'BLENDER_GAME'}

    @classmethod
    def poll(cls, context):
        ob = context.object
        return context.scene.render.engine in cls.COMPAT_ENGINES and ob.type not in {'CAMERA', 'EMPTY', 'LAMP'}

    def draw(self, context):
        layout = self.layout
        ob = context.object
        gs = context.scene.game_settings

        col = layout.column()
        col.prop(ob, "lod_factor", text="Distance Factor")
        
        col = layout.column()
        col.prop(ob, "use_lod_physics", text="Physics Update")

        for i, level in enumerate(ob.lod_levels):
            if i == 0:
                continue
            box = col.box()
            row = box.row(align=True)
            row.prop(level, "object", text="")
            row.prop(level, "use_invisible_mesh", text="")
            row.operator("object.lod_remove", text="", icon='PANEL_CLOSE').index = i
            
            row = box.row()
            row.prop(level, "distance")
            row = row.row(align=True)
            row.prop(level, "use_mesh", text="")
            row.prop(level, "use_material", text="")

            row = box.row()
            row.active = gs.use_scene_hysteresis
            row.prop(level, "use_object_hysteresis", text="Hysteresis Override")
            row = box.row()
            row.active = gs.use_scene_hysteresis and level.use_object_hysteresis
            row.prop(level, "object_hysteresis_percentage", text="")

        row = col.row(align=True)
        row.operator("object.lod_add", text="Add", icon='ZOOMIN')
        row.menu("OBJECT_MT_lod_tools", text="", icon='TRIA_DOWN')
        
class OBJECT_PT_animation_events(ObjectButtonsPanel, Panel):
    bl_label = "Animation Events"
    COMPAT_ENGINES = {'BLENDER_GAME'}

    @classmethod
    def poll(cls, context):
        ob = context.object
        return context.scene.render.engine in cls.COMPAT_ENGINES and ob.type not in {'CAMERA', 'EMPTY', 'LAMP'}

    def draw(self, context):
        layout = self.layout
        ob = context.object
        gs = context.scene.game_settings

        col = layout.column()
        
        for i, event in enumerate(ob.anim_events):
            if i == 0: continue
            box = col.box()
            row = box.row()
            row.prop(event, "show_expanded", text="", emboss=True)
            
            if (event.anim): textHeader = "{EventIndex} - Animation Event: {ActionName}".format(EventIndex=i, ActionName=event.anim.name)
            else: textHeader = "{EventIndex} - Empty Animation Event".format(EventIndex=i)
            row.label(text=textHeader, icon="RECOVER_LAST")
            
            row = row.row(align=True)
            row.operator("object.animation_event_move_up", text="", icon='DOTSUP').index = i
            row.operator("object.animation_event_move_down", text="", icon='DOTSDOWN').index = i
            row.operator("object.animation_event_remove", text="", icon='PANEL_CLOSE').index = i
            
            if (not event.show_expanded):         
                continue
            
            row = box.split(factor=0.2)
            row.label(text="Action:")
            row.prop(event, "anim", text="")
            
            # Triggers
            row = box.split(factor=1)
            row.operator("object.animation_event_trigger_add", text="Add Trigger", icon='ZOOMIN').index = i
            row = box.row()
            if len(event.triggers) == 0: row.label(text="No Triggers! Please add an frame trigger", icon="PANEL_CLOSE")
            else: row.label(text="Triggers", icon="ANIM")
            for it, eventTrigger in enumerate(event.triggers):
                if it == 0: continue
                #row = box.split(factor=1)
                row = box.row()
                row.prop(eventTrigger, "frame", text="Frame")
                row.prop(eventTrigger, "custom_arg", text="")
                
                buttonPick = row.operator("object.animation_event_trigger_pick", text="", icon="EYEDROPPER")
                buttonPick.index = it
                buttonPick.eventIndex = i

                button = row.operator("object.animation_event_trigger_remove", text="", icon="PANEL_CLOSE")
                button.index = it
                button.eventIndex = i
               
            row = box.row()
            row = row.separator(factor=1)
            row = box.row()
            row.label(text="Python Event:", icon="FILE_SCRIPT")
            row.prop(event, "eventcall", text="")

        row = col.row(align=True)
        row.operator("object.animation_event_add", text="Add", icon='ZOOMIN')

classes = (
    GAME_PT_game_components,
    GAME_PT_game_properties,
    PHYSICS_PT_game_physics,
    PHYSICS_PT_game_collision_bounds,
    PHYSICS_PT_game_obstacles,
    RENDER_PT_embedded,
    RENDER_PT_game_player,
    RENDER_PT_game_stereo,
    RENDER_PT_game_shading,
    RENDER_PT_game_post_process_shaders,
    RENDER_PT_game_system,
    RENDER_PT_game_attachments,
    RENDER_PT_game_animations,
    RENDER_PT_game_display,
    RENDER_PT_game_debug,
	RENDER_UL_attachments,
    SCENE_PT_game_physics,
    SCENE_PT_game_physics_obstacles,
    SCENE_PT_game_navmesh,
    SCENE_PT_game_hysteresis,
    SCENE_PT_game_console,
    SCENE_PT_game_audio,
    WORLD_PT_game_context_world,
    WORLD_PT_game_world,
    WORLD_PT_game_environment_lighting,
    WORLD_PT_game_mist,
    DATA_PT_shadow_game,
    OBJECT_MT_lod_tools,
    OBJECT_PT_game_object_tasks,
    OBJECT_MT_culling,
    OBJECT_PT_activity_culling,
    OBJECT_PT_levels_of_detail,
    OBJECT_PT_animation_events, 
)

if __name__ == "__main__":  # only for live edit.
    from bpy.utils import register_class
    for cls in classes:
        register_class(cls)
