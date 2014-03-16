/*
===========================================================================
Copyright (C) 1999-2010 id Software LLC, a ZeniMax Media company.

This file is part of Spearmint Source Code.

Spearmint Source Code is free software; you can redistribute it
and/or modify it under the terms of the GNU General Public License as
published by the Free Software Foundation; either version 3 of the License,
or (at your option) any later version.

Spearmint Source Code is distributed in the hope that it will be
useful, but WITHOUT ANY WARRANTY; without even the implied warranty of
MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
GNU General Public License for more details.

You should have received a copy of the GNU General Public License
along with Spearmint Source Code.  If not, see <http://www.gnu.org/licenses/>.

In addition, Spearmint Source Code is also subject to certain additional terms.
You should have received a copy of these additional terms immediately following
the terms and conditions of the GNU General Public License.  If not, please
request a copy in writing from id Software at the address below.

If you have questions concerning this license or the applicable additional
terms, you may contact in writing id Software LLC, c/o ZeniMax Media Inc.,
Suite 120, Rockville, Maryland 20850 USA.
===========================================================================
*/

// tr_cmesh.c: compressed triangle model functions
//
// This is ripped from tr_mesh.c, and converted to use the compressed mesh format

#include "tr_local.h"

/*
=============
R_CullModel
=============
*/
static int R_CullModel( mdcHeader_t *header, trRefEntity_t *ent ) {
	vec3_t		bounds[2];
	md3Frame_t	*oldFrame, *newFrame;
	int			i;

	// compute frame pointers
	newFrame = ( md3Frame_t * ) ( ( byte * ) header + header->ofsFrames ) + ent->e.frame;
	oldFrame = ( md3Frame_t * ) ( ( byte * ) header + header->ofsFrames ) + ent->e.oldframe;

	// cull bounding sphere ONLY if this is not an upscaled entity
	if ( !ent->e.nonNormalizedAxes )
	{
		if ( ent->e.frame == ent->e.oldframe )
		{
			switch ( R_CullLocalPointAndRadius( newFrame->localOrigin, newFrame->radius ) )
			{
			case CULL_OUT:
				tr.pc.c_sphere_cull_md3_out++;
				return CULL_OUT;

			case CULL_IN:
				tr.pc.c_sphere_cull_md3_in++;
				return CULL_IN;

			case CULL_CLIP:
				tr.pc.c_sphere_cull_md3_clip++;
				break;
			}
		}
		else
		{
			int sphereCull, sphereCullB;

			sphereCull  = R_CullLocalPointAndRadius( newFrame->localOrigin, newFrame->radius );
			if ( newFrame == oldFrame ) {
				sphereCullB = sphereCull;
			} else {
				sphereCullB = R_CullLocalPointAndRadius( oldFrame->localOrigin, oldFrame->radius );
			}

			if ( sphereCull == sphereCullB )
			{
				if ( sphereCull == CULL_OUT )
				{
					tr.pc.c_sphere_cull_md3_out++;
					return CULL_OUT;
				}
				else if ( sphereCull == CULL_IN )
				{
					tr.pc.c_sphere_cull_md3_in++;
					return CULL_IN;
				}
				else
				{
					tr.pc.c_sphere_cull_md3_clip++;
				}
			}
		}
	}
	
	// calculate a bounding box in the current coordinate system
	for (i = 0 ; i < 3 ; i++) {
		bounds[0][i] = oldFrame->bounds[0][i] < newFrame->bounds[0][i] ? oldFrame->bounds[0][i] : newFrame->bounds[0][i];
		bounds[1][i] = oldFrame->bounds[1][i] > newFrame->bounds[1][i] ? oldFrame->bounds[1][i] : newFrame->bounds[1][i];
	}

	switch ( R_CullLocalBox( bounds ) )
	{
	case CULL_IN:
		tr.pc.c_box_cull_md3_in++;
		return CULL_IN;
	case CULL_CLIP:
		tr.pc.c_box_cull_md3_clip++;
		return CULL_CLIP;
	case CULL_OUT:
	default:
		tr.pc.c_box_cull_md3_out++;
		return CULL_OUT;
	}
}


/*
=================
R_ComputeFogNum

=================
*/
static int R_ComputeFogNum( mdcHeader_t *header, trRefEntity_t *ent ) {
	md3Frame_t		*md3Frame;
	vec3_t			localOrigin;

	if ( tr.refdef.rdflags & RDF_NOWORLDMODEL ) {
		return 0;
	}

	// FIXME: non-normalized axis issues
	md3Frame = ( md3Frame_t * ) ( ( byte * ) header + header->ofsFrames ) + ent->e.frame;
	VectorAdd( ent->e.origin, md3Frame->localOrigin, localOrigin );

	return R_PointFogNum( &tr.refdef, localOrigin, md3Frame->radius );
}

/*
=================
R_AddMDCSurfaces

=================
*/
void R_AddMDCSurfaces( trRefEntity_t *ent ) {
	int				i, j;
	mdcHeader_t		*header = NULL;
	mdcSurface_t	*surface = NULL;
	md3Shader_t		*md3Shader = NULL;
	shader_t		*shader = NULL;
	int				cull;
	int				lod;
	int				fogNum;
	qboolean		personalModel;

	// don't add mirror only objects if not in a mirror/portal
	personalModel = (ent->e.renderfx & RF_ONLY_MIRROR) && !tr.viewParms.isPortal;

	if ( ent->e.renderfx & RF_WRAP_FRAMES ) {
		ent->e.frame %= tr.currentModel->mdc[0]->numFrames;
		ent->e.oldframe %= tr.currentModel->mdc[0]->numFrames;
	}

	//
	// Validate the frames so there is no chance of a crash.
	// This will write directly into the entity structure, so
	// when the surfaces are rendered, they don't need to be
	// range checked again.
	//
	if ( (ent->e.frame >= tr.currentModel->mdc[0]->numFrames) 
		|| (ent->e.frame < 0)
		|| (ent->e.oldframe >= tr.currentModel->mdc[0]->numFrames)
		|| (ent->e.oldframe < 0) ) {
			ri.Printf( PRINT_DEVELOPER, "R_AddMDCSurfaces: no such frame %d to %d for '%s'\n",
				ent->e.oldframe, ent->e.frame,
				tr.currentModel->name );
			ent->e.frame = 0;
			ent->e.oldframe = 0;
	}

	//
	// compute LOD
	//
	lod = R_ComputeLOD( ent );

	header = tr.currentModel->mdc[lod];

	//
	// cull the entire model if merged bounding box of both frames
	// is outside the view frustum.
	//
	cull = R_CullModel ( header, ent );
	if ( cull == CULL_OUT ) {
		return;
	}

	//
	// set up lighting now that we know we aren't culled
	//
	if ( !personalModel || r_shadows->integer > 1 ) {
		R_SetupEntityLighting( &tr.refdef, ent );
	}

	//
	// see if we are in a fog volume
	//
	fogNum = R_ComputeFogNum( header, ent );

	//
	// draw all surfaces
	//
	surface = (mdcSurface_t *)( (byte *)header + header->ofsSurfaces );
	for ( i = 0 ; i < header->numSurfaces ; i++ ) {

		if ( ent->e.customShader ) {
			shader = R_GetShaderByHandle( ent->e.customShader );
		} else if ( ent->e.customSkin > 0 && ent->e.customSkin <= tr.refdef.numSkins ) {
			skin_t *skin;
			skinSurface_t *skinSurf;

			skin = &tr.refdef.skins[ent->e.customSkin - 1];

			// match the surface name to something in the skin
			shader = tr.defaultShader;
			for ( j = 0 ; j < skin->numSurfaces ; j++ ) {
				skinSurf = &tr.skinSurfaces[ skin->surfaces[ j ] ];
				// the names have both been lowercased
				if ( !strcmp( skinSurf->name, surface->name ) ) {
					shader = skinSurf->shader;
					break;
				}
			}

			if ( shader == tr.nodrawShader ) {
				surface = (mdcSurface_t *)( (byte *)surface + surface->ofsEnd );
				continue;
			}
		} else if ( surface->numShaders <= 0 ) {
			shader = tr.defaultShader;
		} else {
			md3Shader = (md3Shader_t *) ( (byte *)surface + surface->ofsShaders );
			md3Shader += ent->e.skinNum % surface->numShaders;
			shader = tr.shaders[ md3Shader->shaderIndex ];
		}


		// we will add shadows even if the main object isn't visible in the view

		// stencil shadows can't do personal models unless I polyhedron clip
		if ( !personalModel
			&& r_shadows->integer == 2 
			&& fogNum == 0
			&& !(ent->e.renderfx & ( RF_NOSHADOW | RF_DEPTHHACK ) ) 
			&& shader->sort == SS_OPAQUE ) {
			R_AddDrawSurf( (void *)surface, tr.shadowShader, 0, qfalse );
		}

		// projection shadows work fine with personal models
		if ( r_shadows->integer == 3
			&& fogNum == 0
			&& (ent->e.renderfx & RF_SHADOW_PLANE )
			&& shader->sort == SS_OPAQUE ) {
			R_AddDrawSurf( (void *)surface, tr.projectionShadowShader, 0, qfalse );
		}

		// don't add third_person objects if not viewing through a portal
		if ( !personalModel ) {
			R_AddEntDrawSurf( ent, (void *)surface, shader, fogNum, qfalse );
		}

		surface = (mdcSurface_t *)( (byte *)surface + surface->ofsEnd );
	}

}

