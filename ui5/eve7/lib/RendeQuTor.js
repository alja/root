import {RenderQueue} from './RC/renderers/RenderQueue.js';
import {RenderPass}  from './RC/renderers/RenderPass.js';
import {CustomShaderMaterial} from './RC/materials/CustomShaderMaterial.js';
import {FRONT_AND_BACK_SIDE, HIGHPASS_MODE_BRIGHTNESS, HIGHPASS_MODE_DIFFERENCE}
    from './RC/constants.js';

export class RendeQuTor
{
    constructor(renderer, scene, camera)
    {
        this.renderer = renderer;
        this.scene    = scene;
        this.camera   = camera;
        this.queue    = new RenderQueue(renderer);
        this.pqueue   = new RenderQueue(renderer);
        this.vp_w = 0;
        this.vp_h = 0;
        this.pick_radius = 32;
        this.pick_center = 16;

        this.make_PRP_plain();
        this.make_PRP_depth2r();

        this.SSAA_value = 1;

        const nearPlane = 0.0625; // XXXX - pass to view_setup(vport, nfclip)
        const farPlane  = 8192;   // XXXX

        // For every pass, store object + resize behaviour
    }

    initDirectToScreen()
    {
        this.make_RP_DirectToScreen();
    }

    initSimple(ssaa_val)
    {
        this.SSAA_value = ssaa_val;

        this.make_RP_SSAA_Super();

        this.make_RP_GBuffer();
        this.make_RP_Outline();

        this.make_RP_GaussHVandBlend();

        // this.make_RP_ToScreen();
        // // this.RP_ToneMapToScreen.input_texture = "color_main";
        // this.RP_ToScreen.input_texture = "color_final";

        this.make_RP_ToneMapToScreen();
        //this.RP_ToneMapToScreen.input_texture = "color_main";
        this.RP_ToneMapToScreen.input_texture = "color_final";
        //this.RP_ToneMapToScreen.input_texture = "depth_gbuff";

        this.RP_GBuffer.obj_list = [];
    }

    initFull(ssaa_val)
    {
        this.SSAA_value = ssaa_val;

        this.make_RP_SSAA_Super();
        this.make_RP_HighPassGaussBloom();
        // this.make_RP_SSAA_Down(); this.RP_SSAA_Down.input_texture = "color_bloom";
        this.make_RP_ToScreen();
        this.RP_ToScreen.input_texture = "color_bloom";
    }

    updateViewport(w, h)
    {
        this.vp_w = w;
        this.vp_h = h;
        let vp = { width: w, height: h };
        let rq = this.queue._renderQueue;
        for (let i = 0; i < rq.length; i++)
        {
            rq[i].view_setup(vp);
        }
        // Picking render-passes stay constant.
    }

    render()
    {
        this.queue.render();
    }

    pick_begin(x, y)
    {
        this.camera.prePickStoreTBLR();
        this.camera.narrowProjectionForPicking(this.vp_w, this.vp_h,
                                               this.pick_radius, this.pick_radius,
                                               x, this.vp_h - 1 - y);
    }

    pick_end()
    {
        this.camera.postPickRestoreTBLR();
    }

    pick(x, y, detect_depth = false)
    {
        this.renderer.pick_setup(this.pick_center, this.pick_center);

        let state = this.pqueue.render();
        state.x = x;
        state.y = y;
        state.depth = -1.0;
        state.object = this.renderer.pickedObject3D;
        // console.log("RenderQuTor::pick", state);

        if (detect_depth && this.renderer.pickedObject3D !== null)
        {
            let rdr = this.renderer;
            let x = rdr._pickCoordinateX;
            let y = rdr._canvas.height - rdr._pickCoordinateY;

            let d = new Float32Array(9);
            rdr.gl.readBuffer(rdr.gl.COLOR_ATTACHMENT0);
            rdr.gl.readPixels(this.pick_center - 1, this.pick_center - 1, 3, 3, gl.RED, gl.FLOAT, d);

            let near = this.camera.near;
            let far  = this.camera.far;
            for (let i = 0; i < 9; ++i)
                d[i] = (2.0 * near * far) / (far + near - d[i] * (far - near));

            console.log("Pick depth at", x, ",", y, ":", d);

            state.depth = d[4];
        }

        return state;
    }

    pick_instance(state) {
        // RCRC Proto-proto-secondary-selection. Requires branch:
        // https://github.com/osschar/RenderCore/tree/img-tex-cache
        if (state.object !== this.renderer.pickedObject3D) {
            console.error("RendeQuTor::pick_instance state mismatch", state, this.renderer.pickedObject3D);
        } else {
            console.log("RenderQuTor::pick going for secondary select");

            this.renderer._pickSecondaryEnabled = true;
            this.pqueue.render();

            state.instance = this.renderer._pickedID;
        }
        return state;
    }


    //=============================================================================
    // Picking RenderPasses
    //=============================================================================

    make_PRP_plain()
    {
        var pthis = this;

        this.PRP_plain = new RenderPass(
            RenderPass.BASIC,
            function (textureMap, additionalData) {},
            function (textureMap, additionalData) {
                // pthis.renderer._specialRenderFoo = "Choopacabra";
                return { scene: pthis.scene, camera: pthis.camera };
            },
            function (textureMap, additionalData) {},
            RenderPass.TEXTURE,
            { width: this.pick_radius, height: this.pick_radius },
            "depth_picking",
            [ { id: "color_picking", textureConfig: RenderPass.DEFAULT_R32UI_TEXTURE_CONFIG,
                clearColorArray: new Uint32Array([0xffffffff, 0, 0, 0]) } ]
        );

        this.pqueue.pushRenderPass(this.PRP_plain);
    }

    make_PRP_depth2r()
    {
        this.PRP_depth2r_mat = new CustomShaderMaterial("copyDepth2RReve");
        this.PRP_depth2r_mat.lights = false;
        var pthis = this;

        this.PRP_depth2r = new RenderPass(
            RenderPass.POSTPROCESS,
            function (textureMap, additionalData) {},
            function (textureMap, additionalData) {
                return { material: pthis.PRP_depth2r_mat, textures: [ textureMap["depth_picking"] ] };
            },
            function (textureMap, additionalData) {},
            RenderPass.TEXTURE,
            { width: this.pick_radius, height: this.pick_radius },
            null,
            [ { id: "depthr32f_picking", textureConfig: RenderPass.FULL_FLOAT_R32F_TEXTURE_CONFIG,
                clearColorArray: new Float32Array([1, 0, 0, 0]) } ]
        );

        this.pqueue.pushRenderPass(this.PRP_depth2r);
    }


    //=============================================================================
    // Regular RenderPasses
    //=============================================================================

    make_RP_DirectToScreen()
    {
        var pthis = this;

        this.RP_DirectToScreen = new RenderPass(
            RenderPass.BASIC,
            function (textureMap, additionalData) {},
            function (textureMap, additionalData) { return { scene: pthis.scene, camera: pthis.camera }; },
            function (textureMap, additionalData) {},
            RenderPass.SCREEN,
            null
        );
        this.RP_DirectToScreen.view_setup = function (vport) { this.viewport = vport; };

        this.queue.pushRenderPass(this.RP_DirectToScreen);
    }

    //=============================================================================

    make_RP_SSAA_Super()
    {
        var pthis = this;

        this.RP_SSAA_Super = new RenderPass(
            // Rendering pass type
            RenderPass.BASIC,
            // Initialize function
            function (textureMap, additionalData) {},
            // Preprocess function
            function (textureMap, additionalData) { return { scene: pthis.scene, camera: pthis.camera }; },
            // Postprocess
            function (textureMap, additionalData) {},
            // Target
            RenderPass.TEXTURE,
            // Viewport
            null,
            // Bind depth texture to this ID
            "depth_main",
            // Outputs
            [ { id: "color_main", textureConfig: RenderPass.DEFAULT_RGBA16F_TEXTURE_CONFIG } ]
        );
        this.RP_SSAA_Super.view_setup = function (vport) { this.viewport = { width: vport.width*pthis.SSAA_value, height: vport.height*pthis.SSAA_value }; };

        this.queue.pushRenderPass(this.RP_SSAA_Super);
    }

    make_RP_SSAA_Down()
    {
        this.RP_SSAA_Down_mat = new CustomShaderMaterial("copyTexture");
        this.RP_SSAA_Down_mat.lights = false;
        let pthis = this;

        this.RP_SSAA_Down = new RenderPass(
            // Rendering pass type
            RenderPass.POSTPROCESS,

            // Initialize function
            function (textureMap, additionalData) {},
            // Preprocess function
            function (textureMap, additionalData) {
                return { material: pthis.RP_SSAA_Down_mat, textures: [textureMap[this.input_texture]] };
            },
            // Postprocess function
            function (textureMap, additionalData) {},

            // Target
            RenderPass.TEXTURE,

            // Viewport
            null,

            // Bind depth texture to this ID
            null,

            [ { id: "color_main", textureConfig: RenderPass.DEFAULT_RGBA16F_TEXTURE_CONFIG } ]
        );
        this.RP_SSAA_Down.input_texture = "color_super";
        this.RP_SSAA_Down.view_setup = function(vport) { this.viewport = vport; };

        this.queue.pushRenderPass(this.RP_SSAA_Down);
    }

    //=============================================================================

    make_RP_ToScreen()
    {
        this.RP_ToScreen_mat = new CustomShaderMaterial("copyTexture");
        this.RP_ToScreen_mat.lights = false;
        let pthis = this;

        this.RP_ToScreen = new RenderPass(
            RenderPass.POSTPROCESS,
            function (textureMap, additionalData) {},
            function (textureMap, additionalData) {
                return { material: pthis.RP_ToScreen_mat, textures: [ textureMap[this.input_texture] ] };
            },
            function (textureMap, additionalData) {},
            RenderPass.SCREEN,
            null
        );
        this.RP_ToScreen.input_texture = "color_main";
        this.RP_ToScreen.view_setup = function(vport) { this.viewport = vport; };

        this.queue.pushRenderPass(this.RP_ToScreen);
    }

    make_RP_ToneMapToScreen()
    {
        this.RP_ToneMapToScreen_mat = new CustomShaderMaterial("ToneMapping",
            { MODE: 1.0, gamma: 1.0, exposure: 2.0 });
            // u_clearColor set from MeshRenderer
        this.RP_ToneMapToScreen_mat.lights = false;

        let pthis = this;

        this.RP_ToneMapToScreen = new RenderPass(
            RenderPass.POSTPROCESS,
            function (textureMap, additionalData) {},
            function (textureMap, additionalData) {
                return { material: pthis.RP_ToneMapToScreen_mat,
                         textures: [ textureMap[this.input_texture] ] };
            },
            function (textureMap, additionalData) {},
            RenderPass.SCREEN,
            null
        );
        this.RP_ToneMapToScreen.input_texture = "color_main";
        this.RP_ToneMapToScreen.view_setup = function(vport) { this.viewport = vport; };

        this.queue.pushRenderPass(this.RP_ToneMapToScreen);
    }

    //=============================================================================

    make_RP_HighPassGaussBloom()
    {
        var pthis = this;
        // let hp = new CustomShaderMaterial("highPass", {MODE: HIGHPASS_MODE_BRIGHTNESS, targetColor: [0.2126, 0.7152, 0.0722], threshold: 0.75});
        let hp = new CustomShaderMaterial("highPass", { MODE: HIGHPASS_MODE_DIFFERENCE,
                                             targetColor: [0x0/255, 0x0/255, 0xff/255], threshold: 0.1});
        console.log("XXXXXXXX", hp);
        // let hp = new CustomShaderMaterial("highPassReve");
        this.RP_HighPass_mat = hp;
        this.RP_HighPass_mat.lights = false;

        this.RP_HighPass = new RenderPass(
            RenderPass.POSTPROCESS,
            function (textureMap, additionalData) {},
            function (textureMap, additionalData) {
                return { material: pthis.RP_HighPass_mat, textures: [textureMap["color_ssaa_super"]] };
            },
            function (textureMap, additionalData) {},
            RenderPass.TEXTURE,
            null,
            // XXXXXX MT: this was "dt", why not null ????
            null, // "dt",
            [ {id: "color_high_pass", textureConfig: RenderPass.DEFAULT_RGBA_TEXTURE_CONFIG} ]
        );
        this.RP_HighPass.view_setup = function (vport) { this.viewport = { width: vport.width*pthis.SSAA_value, height: vport.height*pthis.SSAA_value }; };
        this.queue.pushRenderPass(this.RP_HighPass);

        this.RP_Gauss1_mat = new CustomShaderMaterial("gaussBlur", {horizontal: true, power: 1.0});
        this.RP_Gauss1_mat.lights = false;

        this.RP_Gauss1 = new RenderPass(
            RenderPass.POSTPROCESS,
            function (textureMap, additionalData) {},
            function (textureMap, additionalData) {
                return { material: pthis.RP_Gauss1_mat, textures: [textureMap["color_high_pass"]] };
            },
            function (textureMap, additionalData) {},
            RenderPass.TEXTURE,
            null,
            null,
            [ {id: "color_gauss_half", textureConfig: RenderPass.DEFAULT_RGBA_TEXTURE_CONFIG} ]
        );
        this.RP_Gauss1.view_setup = function (vport) { this.viewport = { width: vport.width*pthis.SSAA_value, height: vport.height*pthis.SSAA_value }; };
        this.queue.pushRenderPass(this.RP_Gauss1);

        this.RP_Gauss2_mat = new CustomShaderMaterial("gaussBlur", {horizontal: false, power: 1.0});
        this.RP_Gauss2_mat.lights = false;

        this.RP_Gauss2 = new RenderPass(
            RenderPass.POSTPROCESS,
            function (textureMap, additionalData) {},
            function (textureMap, additionalData) {
                return { material: pthis.RP_Gauss2_mat, textures: [textureMap["color_gauss_half"]] };
            },
            function (textureMap, additionalData) {},
            RenderPass.TEXTURE,
            null,
            null,
            [ {id: "color_gauss_full", textureConfig: RenderPass.DEFAULT_RGBA_TEXTURE_CONFIG} ]
        );
        this.RP_Gauss2.view_setup = function (vport) { this.viewport = { width: vport.width*pthis.SSAA_value, height: vport.height*pthis.SSAA_value }; };
        this.queue.pushRenderPass(this.RP_Gauss2);

        this.RP_Bloom_mat = new CustomShaderMaterial("bloom");
        this.RP_Bloom_mat.lights = false;

        this.RP_Bloom = new RenderPass(
            RenderPass.POSTPROCESS,
            function (textureMap, additionalData) {},
            function (textureMap, additionalData) {
                return { material: pthis.RP_Bloom_mat, textures: [textureMap["color_gauss_full"], textureMap["color_ssaa_super"]] };
            },
            function (textureMap, additionalData) {},
            RenderPass.TEXTURE,
            null,
            null,
            [ {id: "color_bloom", textureConfig: RenderPass.DEFAULT_RGBA_TEXTURE_CONFIG} ]
        );
        this.RP_Bloom.view_setup = function (vport) { this.viewport = { width: vport.width*pthis.SSAA_value, height: vport.height*pthis.SSAA_value }; };
        this.queue.pushRenderPass(this.RP_Bloom);
    }

    //=============================================================================

    make_RP_GBuffer()
    {
        this.RP_GBuffer_mat = new CustomShaderMaterial("GBufferMini");
        this.RP_GBuffer_mat.lights = false;
        this.RP_GBuffer_mat.side = FRONT_AND_BACK_SIDE;

        this.RP_GBuffer_mat_flat = new CustomShaderMaterial("GBufferMini");
        this.RP_GBuffer_mat_flat.lights = false;
        this.RP_GBuffer_mat_flat.side = FRONT_AND_BACK_SIDE;
        this.RP_GBuffer_mat_flat.normalFlat = true;

        let pthis = this;
        let clear_arr = new Float32Array([0,0,0,0]);

        this.RP_GBuffer = new RenderPass(
            RenderPass.BASIC,
            function (textureMap, additionalData) {},
            function (textureMap, additionalData) {
                pthis.renderer._outlineEnabled = true;
                pthis.renderer._outlineArray = this.obj_list;
                pthis.renderer._defaultOutlineMat = pthis.RP_GBuffer_mat;
                pthis.renderer._defaultOutlineMatFlat = pthis.RP_GBuffer_mat_flat;
                pthis.renderer._fillRequiredPrograms(pthis.RP_GBuffer_mat.requiredProgram(pthis.renderer));
                pthis.renderer._fillRequiredPrograms(pthis.RP_GBuffer_mat_flat.requiredProgram(pthis.renderer));
                for (const o3d of this.obj_list) {
                    if (o3d.outlineMaterial)
                        pthis.renderer._fillRequiredPrograms(o3d.outlineMaterial.requiredProgram(pthis.renderer));
                }
                return { scene: pthis.scene, camera: pthis.camera };
            },
            function (textureMap, additionalData) {
                pthis.renderer._outlineEnabled = false; // can remain true if not all progs are loaded
                pthis.renderer._outlineArray = null;
            },
            RenderPass.TEXTURE,
            null,
            "depth_gbuff",
            [
                {id: "normal",  textureConfig: RenderPass.DEFAULT_RGBA16F_TEXTURE_CONFIG,
                 clearColorArray: clear_arr},
                {id: "viewDir", textureConfig: RenderPass.DEFAULT_RGBA16F_TEXTURE_CONFIG,
                 clearColorArray: clear_arr}
            ]
        );
        this.RP_GBuffer.view_setup = function (vport) { this.viewport = vport; };

        // TODO: No push, GBuffer/Outline passes should be handled separately as there can be more of them.
        this.queue.pushRenderPass(this.RP_GBuffer);
        return this.RP_GBuffer;
    }

    make_RP_Outline()
    {
        this.RP_Outline_mat = new CustomShaderMaterial("outline",
          { scale: 1.0,
            edgeColor: [ 1.4, 0.0, 0.8, 1.0 ],
            _DepthThreshold: 6.0,
            _NormalThreshold: 0.6, // 0.4,
            _DepthNormalThreshold: 0.5,
            _DepthNormalThresholdScale: 7.0 });
        this.RP_Outline_mat.lights = false;

        let pthis = this;

        this.RP_Outline = new RenderPass(
            RenderPass.POSTPROCESS,
            function (textureMap, additionalData) {},
            function (textureMap, additionalData) {
                return { material: pthis.RP_Outline_mat,
                         textures: [ textureMap["depth_gbuff"], textureMap["normal"],
                                     textureMap["viewDir"] ] };
            },
            function (textureMap, additionalData) {},
            RenderPass.TEXTURE,
            null,
            null,
            [
                {id: "color_outline", textureConfig: RenderPass.DEFAULT_RGBA16F_TEXTURE_CONFIG}
            ]
        );
        this.RP_Outline.view_setup = function (vport) { this.viewport = vport; };

        // TODO: No push, GBuffer/Outline passes should be handled separately as there can be more of them.
        this.queue.pushRenderPass(this.RP_Outline);
        return this.RP_Outline;
    }

    make_RP_GaussHVandBlend()
    {
        let pthis = this;

        this.RP_GaussH_mat = new CustomShaderMaterial("gaussBlur", {horizontal: true, power: 4.0});
        this.RP_GaussH_mat.lights = false;

        this.RP_GaussH = new RenderPass(
            RenderPass.POSTPROCESS,
            (textureMap, additionalData) => {},
            (textureMap, additionalData) => {
                return {material: pthis.RP_GaussH_mat, textures: [textureMap["color_outline"]]};
            },
            (textureMap, additionalData) => {},
            RenderPass.TEXTURE,
            null,
            null,
            [
                {id: "gauss_h", textureConfig: RenderPass.DEFAULT_RGBA16F_TEXTURE_CONFIG}
            ]
        );
        this.RP_GaussH.view_setup = function (vport) { this.viewport = vport; };

        this.RP_GaussV_mat = new CustomShaderMaterial("gaussBlur", {horizontal: false, power: 4.0});
        this.RP_GaussV_mat.lights = false;

        this.RP_GaussV = new RenderPass(
            RenderPass.POSTPROCESS,
            (textureMap, additionalData) => {},
            (textureMap, additionalData) => {
                return {material: pthis.RP_GaussV_mat, textures: [textureMap["gauss_h"]]};
            },
            (textureMap, additionalData) => {},
            RenderPass.TEXTURE,
            null,
            null,
            [
                {id: "gauss_hv", textureConfig: RenderPass.DEFAULT_RGBA16F_TEXTURE_CONFIG}
            ]
        );
        this.RP_GaussV.view_setup = function (vport) { this.viewport = vport; };

        this.RP_Blend_mat = new CustomShaderMaterial("blendingAdditive");
        this.RP_Blend_mat.lights = false;

        this.RP_Blend = new RenderPass(
            RenderPass.POSTPROCESS,
            (textureMap, additionalData) => {},
            (textureMap, additionalData) => {
                return {material: pthis.RP_Blend_mat,
                        textures: [textureMap["gauss_hv"],
                                   textureMap["color_main"]]};
            },
            (textureMap, additionalData) => {},
            // Target
            RenderPass.TEXTURE,
            null,
            null,
            [
                {id: "color_final", textureConfig: RenderPass.DEFAULT_RGBA16F_TEXTURE_CONFIG}
            ]
        );
        this.RP_Blend.view_setup = function (vport) { this.viewport = vport; };

        this.queue.pushRenderPass(this.RP_GaussH);
        this.queue.pushRenderPass(this.RP_GaussV);
        this.queue.pushRenderPass(this.RP_Blend);

        return this.RP_Blend;
    }
}
