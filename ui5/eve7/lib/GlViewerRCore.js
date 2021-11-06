sap.ui.define([
   'rootui5/eve7/lib/GlViewer',
   'rootui5/eve7/lib/EveElementsRCore'
], function(GlViewer, EveElements) {

   "use strict";

   let RC, RP, RendeQuTor;

   class GlViewerRCore extends GlViewer {

      constructor(viewer_class)
      {
         super(viewer_class);

         const urlParams = new URLSearchParams(window.location.search);

         console.log("XXXX", window.location.search, urlParams.get('UseRenderQueue'), urlParams.get('NoRenderQueue'));
         this.UseRenderQueue = true;
         if (urlParams.get('UseRenderQueue') != null) this.UseRenderQueue = true;
         if (urlParams.get('NoRenderQueue' ) != null) this.UseRenderQueue = false;

         if (this.UseRenderQueue)
         {
            let mode_mm = /^(?:Direct|Simple|Full)$/.exec(urlParams.get('RQ_Mode'));
            let ssaa_mm = /^(1|2|4)$/.               exec(urlParams.get('RQ_SSAA'));

            this.RQ_Mode = (mode_mm) ? mode_mm[0] : "Simple";
            this.RQ_SSAA = (ssaa_mm) ? ssaa_mm[0] : 2;
         }

         console.log("UseRenderQueue", this.UseRenderQueue, "RQ_Mode", this.RQ_Mode, "RQ_SSAA", this.RQ_SSAA);
      }

      init(controller)
      {
         super.init(controller);

         let pthis = this;

         // For offline mode, one needs a a full URL or the request
         // gets forwarded to openi5.hana.ondemand.com.
         // This has to be understood and fixed. Loading of shaders
         // afterwards fails, too.
         // // console.log(window.location.pathname); // where are we loading from?
         // import("https://desire.physics.ucsd.edu/matevz/alja.github.io/rootui5/eve7/rnr_core/RenderCore.js").then((module) => {

         import("../../eve7/lib/REveRenderCore.js").then((module) => {
            console.log("GlViewerRCore.onInit - RenderCore.js loaded");

            RC = module;
            pthis.bootstrap();
         });
      }

      bootstrap()
      {
         this.creator = new EveElements(RC);
         // this.creator.useIndexAsIs = EVE.JSR.decodeUrl().has('useindx');

         RC.Object3D.sDefaultPickable = false;

         this.createRCoreRenderer();
         this.controller.createScenes();
         this.controller.redrawScenes();
         this.setupEventHandlers();

         this.controller.glViewerInitDone();

         // XXXX MT: HACK ... give RCore some time to load shaders.
         // Would be better to have some onShadersLoaded thing but
         // this is probably problematic later on,k if we add objects with
         // custom shaders later on.
         setTimeout(this.render.bind(this), 500);
      }

      //==============================================================================

      make_object(name)
      {
         let c = new RC.Group();
         c.name = name || "<no-name>";
         return c;
      }

      get_top_scene()
      {
         return this.scene;
      }

      //==============================================================================

      createRCoreRenderer()
      {
         let w = this.get_width();
         let h = this.get_height();

         //this.canvas = document.createElementNS( 'http://www.w3.org/1999/xhtml', 'canvas' );
         this.canvas = document.createElement('canvas');
         this.canvas.id     = "rcore-canvas";
         this.canvas.width  = w;
         this.canvas.height = h;
         this.get_view().getDomRef().appendChild(this.canvas);

         this.canvas.canvasDOM = this.canvas; // RCore wants this in GLManager ctor ?

         let gl = this.canvas.getContext("webgl2");

         this.renderer = new RC.MeshRenderer(this.canvas, RC.WEBGL2, {antialias: false, stencil: true});
         this.renderer.clearColor = "#FFFFFFFF";
         this.renderer.addShaderLoaderUrls("rootui5sys/eve7/lib/RC/shaders");
         this.renderer.addShaderLoaderUrls("rootui5sys/eve7/shaders");
         this.renderer.pickObject3D = true;
         this.renderer.pickDoNotRender = true;

         this.scene = new RC.Scene();

         this.lights = this.make_object("Light container");
         this.scene.add(this.lights);

         let a_light = new RC.AmbientLight(new RC.Color(0xffffff), 0.1);
         this.lights.add(a_light);

         let light_class_3d = RC.PointLight; // RC.DirectionalLight; // RC.PointLight;
         let light_class_2d = RC.DirectionalLight;

         if (this.controller.isEveCameraPerspective())
         {
            this.camera = new RC.PerspectiveCamera(75, w / h, 20, 4000);
            this.camera.position = new RC.Vector3(-500, 0, 0);
            this.camera.lookAt(new RC.Vector3(0, 0, 0), new RC.Vector3(0, 1, 0));
            this.camera.isPerspectiveCamera = true;

            let l_int = 0.85;
            this.lights.add(new light_class_3d(0xaa8888, l_int )); // R
            this.lights.add(new light_class_3d(0x88aa88, l_int )); // G
            this.lights.add(new light_class_3d(0x8888aa, l_int )); // B
            this.lights.add(new light_class_3d(0x999999, l_int )); // gray

            // Lights are positioned in resetRenderer.

            // Markers on light positions (screws up bounding box / camera reset calculations)
            // for (let i = 1; i <= 4; ++i)
            // {
            //    let l = this.lights.children[i];
            //    l.add( new RC.IcoSphere(1, 1, 10.0, l.color.clone().multiplyScalar(0.5), false) );
            // }
         }
         else
         {
            this.camera = new RC.OrthographicCamera(-w/2, w/2, -h/2, h/2, 20, 2000);
            this.camera.position = new RC.Vector3(0, 0, 50);
            this.camera.lookAt(new RC.Vector3(0, 0, 0), new RC.Vector3(0, 1, 0));
            this.camera.isOrthographicCamera = true;

            let l_int = 0.85;
            this.lights.add(new light_class_2d( 0xffffff, l_int )); // white front
            this.lights.add(new light_class_2d( 0xffffff, l_int )); // white back

            // Lights are positioned in resetRenderer.
         }

         // Test objects
         if (this.controller.isEveCameraPerspective())
         {
            let c = new RC.Cube(40, new RC.Color(0.2,.4,.8));
            c.material = new RC.MeshPhongMaterial();
            c.material.transparent = true;
            c.material.opacity = 0.8;
            c.material.depthWrite  = false;
            this.scene.add(c);

            let ss = new RC.Stripe([0,0,0, 100,50,50, 100,200,200, 300,300,300]);
            ss.material.lineWidth = 20.0;
            ss.material.color     = new RC.Color(0xff0000);
            ss.material.emissive  = new RC.Color(0x008080);
            this.scene.add(ss);
         }

         this.rot_center = new RC.Vector3(0,0,0);

         if (this.UseRenderQueue)
         {
            this.rqt = new RC.RendeQuTor(this.renderer, this.scene, this.camera);
            if (this.RQ_Mode == "Direct")
            {
               this.rqt.initDirectToScreen();
            }
            else if (this.RQ_Mode == "Simple")
            {
               this.rqt.initSimple(this.RQ_SSAA);
               this.creator.SetupPointLineFacs(this.RQ_SSAA, this.RQ_SSAA);
            }
            else
            {
               this.rqt.initFull(this.RQ_SSAA);
               this.creator.SetupPointLineFacs(this.RQ_SSAA, this.RQ_SSAA);
            }
            this.rqt.updateViewport(w, h);
         }
      }

      setupEventHandlers()
      {
         let dome = this.get_view().getDomRef();

         // Setup tooltip
         this.ttip = document.createElement('div');
         this.ttip.setAttribute('class', 'eve_tooltip');
         this.ttip_text = document.createElement('div');
         this.ttip.appendChild(this.ttip_text);
         dome.appendChild(this.ttip);

         // Setup some event pre-handlers
         let glc = this;

         dome.addEventListener('pointermove', function(event) {

            if (event.movementX == 0 && event.movementY == 0)
               return;

            glc.removeMouseupListener();

            if (event.buttons === 0) {
               glc.removeMouseMoveTimeout();
               glc.mousemove_timeout = setTimeout(glc.onMouseMoveTimeout.bind(glc, event.offsetX, event.offsetY), glc.controller.htimeout);
            } else {
               glc.clearHighlight();
            }
         });

         dome.addEventListener('pointerleave', function() {

            glc.removeMouseMoveTimeout();
            glc.clearHighlight();
            glc.removeMouseupListener();
         });

         dome.addEventListener('pointerdown', function(event) {

            glc.removeMouseMoveTimeout();
            if (event.buttons != 1 && event.buttons != 2)  glc.clearHighlight();
            glc.removeMouseupListener();

            // console.log("GLC::mousedown", this, glc, event, event.offsetX, event.offsetY);

            glc.mouseup_listener = function(event2)
            {
               this.removeEventListener('pointerup', glc.mouseup_listener);

               if (event2.buttons == 1) // Selection on mouseup without move
               {
                  glc.handleMouseSelect(event2);
               }
               else if (event.buttons == 2) // Context menu on delay without move
               {
                  EVE.JSR.createMenu(event2, glc).then(menu => glc.showContextMenu(event2, menu));
               }
            }

            this.addEventListener('pointerup', glc.mouseup_listener);
         });

         dome.addEventListener('dblclick', function() {
            //if (glc.controller.dblclick_action == "Reset")
            glc.resetRenderer();
         });

         // Key-handlers go on window ...

         window.addEventListener('keydown', function(event) {
            // console.log("GLC::keydown", event.key, event.code, event);

            let handled = true;

            if (event.key == "t")
            {
               glc.scene.traverse( function( node ) {

                  if ( node.lineWidth )
                  {
                     if ( ! node.lineWidth_orig) node.lineWidth_orig = node.lineWidth;

                     node.lineWidth *= 1.2;
                  }
               });
            }
            else if (event.key == "e")
            {
               glc.scene.traverse( function( node ) {

                  if ( node.lineWidth )
                  {
                     if ( ! node.lineWidth_orig) node.lineWidth_orig = node.lineWidth;

                     node.lineWidth *= 0.8;
                  }
               });
            }
            else if (event.key == "r")
            {
               glc.scene.traverse( function( node ) {

                  if ( node.lineWidth && node.lineWidth_orig )
                  {
                     node.lineWidth = node.lineWidth_orig;
                  }
               });
            }
            else
            {
               handled = false;
            }

            if (handled)
            {
               // // // event.stopPropagation();
               // event.preventDefault();
               // event.stopImmediatePropagation();

               glc.render();
            }
         });

         // Was RC.ReveCameraControls
         this.controls = new THREE.OrbitControls(this.camera, this.get_view().getDomRef());
         this.controls.addEventListener('change', this.render.bind(this));

         // This will also call render().
         this.resetRenderer();
      }

      resetRenderer()
      {
         let sbbox = new RC.Box3();
         sbbox.setFromObject( this.scene );
         if (sbbox.isEmpty())
         {
            console.error("GlViewerRenderCore.resetRenderer scene bbox empty", sbbox);
            const ext = 100;
            sbbox.expandByPoint(new RC.Vector3(-ext,-ext,-ext));
            sbbox.expandByPoint(new RC.Vector3( ext, ext, ext));
         }

         let posV = new RC.Vector3; posV.subVectors(sbbox.max, this.rot_center);
         let negV = new RC.Vector3; negV.subVectors(sbbox.min, this.rot_center);

         let extV = new RC.Vector3; extV = negV; extV.negate(); extV.max(posV);
         let extR = extV.length();

         console.log("GlViewerRenderCore.resetRenderer", sbbox, posV, negV, extV, extR);

         if (this.camera.isPerspectiveCamera)
         {
            let posC = new RC.Vector3(-0.7 * extR, 0.5 * extR, -0.7 * extR);

            this.camera.position.copy(posC);
            this.camera.lookAt(new RC.Vector3(0,0,0), new RC.Vector3(0,1,0));

            this.controls.screenSpacePanning = true;

            let lc = this.lights.children;
            lc[1].position.set( extR, extR, -extR); lc[1].decay = 4 * extR;
            lc[2].position.set(-extR, extR,  extR); lc[2].decay = 4 * extR;
            lc[3].position.set( extR, extR,  extR); lc[3].decay = 4 * extR;
            lc[4].position.set(-extR, extR, -extR); lc[4].decay = 4 * extR;

            // console.log("resetRenderer 3D scene bbox ", sbbox, ", camera_pos ", posC, ", look_at ", this.rot_center);
         }
         else
         {
            let posC = new RC.Vector3(0, 0, extR);

            this.camera.position.copy(posC);

            let ey = 1.02 * extV.y;
            let ex = ey / this.get_height() * this.get_width();
            this.camera._left   = -ex;
            this.camera._right  =  ex;
            this.camera._top    =  ey;
            this.camera._bottom = -ey;
            this.camera.updateProjectionMatrix();

            if (typeof this.controls.resetOrthoPanZoom == 'function')
               this.controls.resetOrthoPanZoom();

            this.controls.screenSpacePanning = true;
            this.controls.enableRotate = false;

            let lc = this.lights.children;
            lc[1].position.set( 0, 0,  extR);
            lc[2].position.set( 0, 0, -extR);

            // console.log("resetRenderer 2D scene bbox ex ey", sbbox, ex, ey, ", camera_pos ", posC, ", look_at ", this.rot_center);
         }
         this.controls.target.copy( this.rot_center );

         // this.composer.reset();

         this.controls.update();
      }

      //==============================================================================

      render()
      {
         // console.log("RENDER", this.scene, this.camera, this.canvas, this.renderer);

         if (this.canvas.width <= 0 || this.canvas.height <= 0) return;

         if (this.UseRenderQueue)
            this.rqt.render();
         else
            this.renderer.render( this.scene, this.camera );

         // if (this.controller.kind === "3D")
         //    window.requestAnimationFrame(this.render.bind(this));
      }

      render_for_picking(x, y)
      {
         console.log("RENDER FOR PICKING", this.scene, this.camera, this.canvas, this.renderer);

         if (this.canvas.width <= 0 || this.canvas.height <= 0) return;

         this.renderer.pick(x, y);
         this.rqt.pick(this.scene, this.camera);

         let o3d = this.renderer.pickedObject3D;
         // Render to FBO or texture would work.
         // let d   = this.renderer.pickedDepth;
         console.log("pick result", o3d /* , d */);
         return o3d;
      }

      //==============================================================================

      onResizeTimeout()
      {
         let w = this.get_width();
         let h = this.get_height();

         if ( ! this.canvas) {
            console.log("GlViewerRCore onResizeTimeout", w, h, "canvas IS NOT SET, STOP CALLING ME!");
            return;
         }

         //console.log("GlViewerRCore onResizeTimeout", w, h, "canvas=", this.canvas, this.canvas.width, this.canvas.height);

         this.canvas.width  = w;
         this.canvas.height = h;

         this.camera.aspect = w / h;

         this.renderer.updateViewport(w, h);

         if (this.UseRenderQueue) this.rqt.updateViewport(w, h);

         this.controls.update();
         this.render();
      }


      //==============================================================================
      // RCore renderer event handlers etc.
      //==============================================================================

      //------------------------------------------------------------------------------
      // Highlight & Mouse move timeout handling
      //------------------------------------------------------------------------------

      clearHighlight()
      {
         if (this.highlighted_scene)
         {
            this.highlighted_scene.clearHighlight(); // XXXX should go through manager
            this.highlighted_scene = 0;

            this.ttip.style.display = "none";
         }
      }

      removeMouseMoveTimeout()
      {
         if (this.mousemove_timeout)
         {
            clearTimeout(this.mousemove_timeout);
            delete this.mousemove_timeout;
         }
      }

      /** Get three.js intersect object at specified mouse position */
      getIntersectAt(x, y)
      {
         console.log("GLC::onMouseMoveTimeout", x, y);

         let o3d = this.render_for_picking(x, y);
         if (!o3d) return null;
         if (!o3d.get_ctrl) o3d = o3d.parent;
         if (o3d.get_ctrl) {
            if (!o3d.get_ctrl) o3d = o3d.parent;
            let w = this.get_width();
            let h = this.get_height();
            let mouse = new RC.Vector2( ((x + 0.5) / w) * 2 - 1, -((y + 0.5) / h) * 2 + 1 );
            return { object: o3d, mouse: mouse, w: w, h: h };
         }
         return null;
      },

      onMouseMoveTimeout(x, y)
      {
         delete this.mousemove_timeout;

         let intersect = this.getIntersectAt(x,y);

         if ( ! intersect)
            return this.clearHighlight();

         let c = intersect.object.get_ctrl();

         let mouse = intersect.mouse;

         // c.elementHighlighted(c.extractIndex(intersect));

         this.highlighted_scene = c.obj3d.scene;

         if (c.obj3d && c.obj3d.eve_el)
            this.ttip_text.innerHTML = c.getTooltipText(intersect);
         else
            this.ttip_text.innerHTML = "";

         let dome = this.controller.getView().getDomRef();
         let offs = (mouse.x > 0 || mouse.y < 0) ? this.getRelativeOffsets(dome) : null;

         if (mouse.x <= 0) {
            this.ttip.style.left  = (x + dome.offsetLeft + 10) + "px";
            this.ttip.style.right = null;
         } else {
            this.ttip.style.right = (intersect.w - x + offs.right + 10) + "px";
            this.ttip.style.left  = null;
         }
         if (mouse.y >= 0) {
            this.ttip.style.top    = (y + dome.offsetTop + 10) + "px";
            this.ttip.style.bottom = null;
         } else {
            this.ttip.style.bottom = (intersect.h - y + offs.bottom + 10) + "px";
            this.ttip.style.top = null;
         }

         this.ttip.style.display= "block";
      }

      getRelativeOffsets(elem)
      {
         // Based on:
         // https://stackoverflow.com/questions/3000887/need-to-calculate-offsetright-in-javascript

         let r = { left: 0, right: 0, top:0, bottom: 0 };

         let parent = elem.offsetParent;

         while (parent && getComputedStyle(parent).position === 'relative')
         {
            r.top    += elem.offsetTop;
            r.left   += elem.offsetLeft;
            r.right  += parent.offsetWidth  - (elem.offsetLeft + elem.offsetWidth);
            r.bottom += parent.offsetHeight - (elem.offsetTop  + elem.offsetHeight);

            elem   = parent;
            parent = parent.offsetParent;
         }

         return r;
      }

      //------------------------------------------------------------------------------
      // Mouse button handlers, selection, context menu
      //------------------------------------------------------------------------------

      removeMouseupListener()
      {
         if (this.mouseup_listener)
         {
            this.get_view().getDomRef().removeEventListener('pointerup', this.mouseup_listener);
            this.mouseup_listener = 0;
         }
      }

      showContextMenu(event, menu)
      {
         // console.log("GLC::showContextMenu", this, menu)

         // See js/modules/menu/menu.mjs createMenu(), menu.add()

         let intersect = this.getIntersectAt(event.offsetX, event.offsetY);

         menu.add("header:Context Menu");

         if (intersect) {
            if (intersect.object.eve_el)
            menu.add("Browse to " + (intersect.object.eve_el.fName || "element"), intersect.object.eve_el.fElementId, this.controller.invokeBrowseOf.bind(this.controller));
         }

         menu.add("Reset camera", this.resetRenderer);

         menu.add("separator");

         let fff = this.defaultContextMenuAction;
         menu.add("sub:Sub Test");
         menu.add("Foo",     'foo', fff);
         menu.add("Bar",     'bar', fff);
         menu.add("Baz",     'baz', fff);
         menu.add("endsub:");

         menu.show(event);
      }

      defaultContextMenuAction(arg)
      {
         console.log("GLC::defaultContextMenuAction", this, arg);
      }

      handleMouseSelect(event)
      {
         let intersect = this.getIntersectAt(event.offsetX, event.offsetY);

         if (intersect) {
            let c = intersect.object.get_ctrl();
            c.event = event;
            c.elementSelected(c.extractIndex(intersect));
            this.highlighted_scene = intersect.object.scene;
         } else {
            // XXXX HACK - handlersMIR senders should really be in the mgr

            this.controller.created_scenes[0].processElementSelected(null, [], event);
         }
      }

   } // class GlViewerRCore

   return GlViewerRCore;
});
