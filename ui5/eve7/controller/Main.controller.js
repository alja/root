sap.ui.define(['sap/ui/core/Component',
               'sap/ui/core/UIComponent',
               'sap/ui/core/mvc/Controller',
               'sap/ui/layout/Splitter',
               'sap/ui/layout/SplitterLayoutData',
               'sap/m/library',
               'sap/m/Button',
               'sap/m/MenuItem',
               'sap/m/MessageBox',
               'rootui5/eve7/lib/EveManager',
               "sap/ui/core/mvc/XMLView",
               'sap/ui/model/json/JSONModel',
               'sap/m/MessageToast'
], function(Component, UIComponent, Controller, Splitter, SplitterLayoutData, MobileLibrary, mButton, mMenuItem, MessageBox, EveManager, XMLView, JSONModel, MessageToast) {

   "use strict";

   return Controller.extend("rootui5.eve7.controller.Main", {
      onInit: function () {
         this.mgr = new EveManager();
        // this.initClientLog();

         let conn_handle = Component.getOwnerComponentFor(this.getView()).getComponentData().conn_handle;
         this.mgr.UseConnection(conn_handle);
         // this.mgr.UseConnection(this.getView().getViewData().conn_handle);

         this.mgr.RegisterController(this);
         // method to found summary controller by ID and set manager to it
         let elem = this.byId("Summary");
         let ctrl = elem.getController();
         ctrl.SetMgr(this.mgr);

         this.primarySplitter = this.getView().byId("MainAreaSplitter");
         this.layoutSplitters = [];  // splitters created for the window layout
         this.localLayout = null;    // layout modified by user (drag and drop) until the server sends it back, null - use server layout
      },

      onDisconnect : function() {
         let t = this.byId("centerTitle");
         t.setHtmlText("<strong style=\"color: red;\">Client Disconnected !</strong>");
      },

      /** called when relative number of possible send operation below or over the threshold  */
      onSendThresholdChanged: function(below, value) {
         let t = this.byId("centerTitle");
         t.$().css('color', below ? 'yellow' : '')
      },

      initClientLog: function() {
         let consoleObj = {};
         consoleObj.data = [];

         consoleObj.model = new JSONModel();
         consoleObj.model.setData(consoleObj.data);

         consoleObj.cntInfo = 0;
         consoleObj.cntWarn = 0;
         consoleObj.cntErr = 0;

         consoleObj.stdlog = console.log.bind(console);
         console.log = function ()
         {
            consoleObj.data.push({ type: "Information", title: Array.from(arguments), counter: ++consoleObj.cntInfo });
            consoleObj.stdlog.apply(console, arguments);
            consoleObj.model.setData(consoleObj.data);
            consoleObj.model.refresh(true);
         };

         consoleObj.stdwarn = console.warn.bind(console);
         console.warn = function ()
         {
            consoleObj.data.push({ type: "Warning", title: Array.from(arguments), counter: ++consoleObj.cntWarn });
            consoleObj.stdwarn.apply(console, arguments);
            consoleObj.model.setData(consoleObj.data);
            consoleObj.model.refresh(true);
         };

         consoleObj.stderror = console.error.bind(console);
         console.error = function ()
         {
            consoleObj.data.push({ type: "Error", title: Array.from(arguments), counter: ++consoleObj.cntErr });
            consoleObj.stderror.apply(console, arguments);
            consoleObj.model.setData(consoleObj.data);
            consoleObj.model.refresh(true);
         };

         // create GUI ClientLog
         let pthis = this;
         XMLView.create({
            viewName: "rootui5.eve7.view.ClientLog",
         }).then(function (oView)
         {
            oView.setModel(consoleObj.model);
            oView.getController().oDialog.setModel(consoleObj.model);
            let logCtrl = oView.getController();
            let toolbar = pthis.byId("otb1");
            toolbar.addContentRight(logCtrl.getButton());
         });
      },

      UpdateCommandsButtons: function(cmds) {
         if (!cmds || this.commands) return;

         let toolbar = this.byId("otb1");

         this.commands = cmds;
         for (let k = cmds.length-1; k>=0; --k) {
            let btn = new mButton({
               icon: cmds[k].icon,
               text: cmds[k].name,
               press: this.executeCommand.bind(this, cmds[k])
            });
            toolbar.insertContentLeft(btn, 0);
         }
      },

      viewItemPressed: function (elem, oEvent) {
         let item = oEvent.getSource();
         // console.log('item pressed', item.getText(), elem);

         let name = item.getText();
         if (name.indexOf(" ") > 0) name = name.substr(0, name.indexOf(" "));
         // FIXME: one need better way to deliver parameters to the selected view
         EVE.$eve7tmp = { mgr: this.mgr, eveViewerId: elem.fElementId};
         let oRouter = UIComponent.getRouterFor(this);
         if (name == "Table")
            oRouter.navTo("Table", { viewName: name });
         else if (name == "Lego")
            oRouter.navTo("Lego", { viewName: name });
         else
            oRouter.navTo("View", { viewName: name });
      },

      updateViewers: function(loading_done) {
         let viewers = this.mgr.FindViewers();

         // first check number of views to create
         let staged = [];
         for (let n=0;n<viewers.length;++n) {
            let el = viewers[n];
            // at startup show only mandatory views
            if (typeof el.subscribed == 'undefined')
               el.subscribed = el.Mandatory;

            if (!el.$view_created && el.fRnrSelf) staged.push(el);
         }
         if (staged.length == 0) return;

         let vMenu = this.getView().byId("menuViewId");

         for (let n=0;n<staged.length;++n) {
            let eveView = staged[n];

            // add menu item
            let vi = new mMenuItem({ text: staged[n].fName, press: this.subscribeView.bind(this, staged[n]) });
            vi.setEnabled(!eveView.subscribed);
            vi.eveView = eveView;
            vMenu.addItem(vi);

            eveView.$view_created = true;
            if(eveView.subscribed) this.makeEveViewController(eveView, true);
         }

         this.rebuildLayout();
      },

      makeEveViewController: function(elem, skip_layout)
      {
         let viewid = "EveViewer" + elem.fElementId;
         let main = this;
         // create missing view
         console.log("Creating view", viewid);
 
         // TODO: Generalize instantiation without  the if/else statements
         let vtype = "rootui5.eve7.view.GL";
         if (elem.fName === "Table")
            vtype = "rootui5.eve7.view.EveTable";
         else if (elem.fName === "Lego")
            vtype = "rootui5.eve7.view.Lego";
         else if (elem.fName === "GeoTable")
               vtype = "rootui5.eve7.view.GeoTable";
         else if (elem.fName === "GeoOverlapTable")
               vtype = "rootui5.eve7.view.GeoOverlapTable";

         let oOwnerComponent = Component.getOwnerComponentFor(this.getView());
         let view = oOwnerComponent.runAsOwner(function() {
            return new sap.ui.xmlview({
               id: viewid,
               viewName: vtype,
               viewData: { mgr: main.mgr, eveViewerId: elem.fElementId },
            });
         });

         elem.ca = view;
         this.setupViewDragAndDrop(view, elem);

         // reset flag needed by UT_PostStream callback
         delete elem.pendInstance;

         if (!skip_layout)
            this.rebuildLayout();
      },

      //==============================================================================
      // Window layout
      //==============================================================================

      /** Viewer is shown when it is subscribed, its view was created and it is not moved to a separate window */
      isViewerShown: function(viewer)
      {
         return viewer && viewer.subscribed && viewer.ca && viewer.fRnrSelf && !viewer.undocked;
      },

      /** Returns layout tree: REveWindowManager layout with all shown viewers placed.
       *  Without server layout, the first viewer is on the left and the rest are stacked vertically on the right. */
      getEffectiveLayout: function()
      {
         let wm = this.mgr.FindWindowManager();
         let src = this.localLayout || (wm && wm.Layout);
         let layout = src ? JSON.parse(JSON.stringify(src)) : null;

         let placed = new Set();
         let collect = pack => pack.Items.forEach(it => {
            if (it.Pack) collect(it.Pack); else placed.add(it.ViewerId);
         });
         if (layout) collect(layout);

         let rest = this.mgr.FindViewers().filter(v => this.isViewerShown(v) && !placed.has(v.fElementId))
                                          .map(v => ({ Weight: 1, ViewerId: v.fElementId }));

         if (!layout || !layout.Items.length) {
            layout = { Vertical: false, Items: rest.slice(0, 1) };
            if (rest.length > 1)
               layout.Items.push({ Weight: 0.5, Pack: { Vertical: true, Items: rest.slice(1) } });
         } else {
            layout.Items.push(...rest);
         }
         return layout;
      },

      /** Creates control for a layout item, returns null when there is nothing to show.
       *  path - indices of the item's pack from the top of the layout tree */
      buildLayoutItem: function(item, path)
      {
         if (!item.Pack) {
            let viewer = this.mgr.GetElement(item.ViewerId);
            if (!this.isViewerShown(viewer)) return null;
            return viewer.ca;
         }

         let parts = [];
         item.Pack.Items.forEach((it, idx) => {
            let ctrl = this.buildLayoutItem(it, path.concat(idx));
            if (!(it.Weight > 0)) it.Weight = 1;
            if (ctrl) parts.push({ ctrl, idx, weight: it.Weight });
         });

         if (parts.length == 0) return null;
         if (parts.length == 1) return parts[0].ctrl;

         let splitter = new Splitter({ orientation: item.Pack.Vertical ? "Vertical" : "Horizontal",
                                       width: "100%", height: "100%" });
         let sum = parts.reduce((acc, p) => acc + p.weight, 0);
         parts.forEach((p, i) => {
            let size = (i == parts.length - 1) ? "auto" : (100 * p.weight / sum).toFixed(2) + "%";
            p.ctrl.setLayoutData(new SplitterLayoutData({ resizable: true, size }));
            splitter.addContentArea(p.ctrl);
         });
         splitter._evePath = path;
         splitter._eveIdx = parts.map(p => p.idx);
         splitter.attachResize(evt => this.onLayoutSplitterResize(splitter, evt.getParameter("newSizes")));
         this.layoutSplitters.push(splitter);
         return splitter;
      },

      /** Recreate splitters in the main area, right of the summary view. Viewer views are reused. */
      rebuildLayout: function()
      {
         let areas = this.primarySplitter.getContentAreas();
         for (let i = areas.length - 1; i > 0; --i)
            this.primarySplitter.removeContentArea(areas[i]);
         // detach views before destroying splitters
         this.layoutSplitters.forEach(s => s.removeAllContentAreas());
         this.layoutSplitters.forEach(s => s.destroy());
         this.layoutSplitters = [];

         this.shownLayout = this.getEffectiveLayout();
         let root = this.buildLayoutItem({ Pack: this.shownLayout }, []);
         this.shownLayoutKey = this.layoutKey(this.shownLayout);
         if (root) {
            root.setLayoutData(new SplitterLayoutData({ resizable: true, size: "auto" }));
            this.primarySplitter.addContentArea(root);
         }
      },

      /** Key to compare layout trees, weights are rounded as the server keeps them as float */
      layoutKey: function(layout)
      {
         let key = p => (p.Vertical ? "V(" : "H(") + p.Items.map(it => (it.Pack ? key(it.Pack) : it.ViewerId) + ":" +
                                                    Math.round((it.Weight > 0 ? it.Weight : 1) * 1e4)).join(",") + ")";
         return key(layout);
      },

      onWindowLayoutChanged: function()
      {
         this.localLayout = null;
         // layout sent by sendLayout() comes back unchanged, views are already in place
         if (this.layoutKey(this.getEffectiveLayout()) !== this.shownLayoutKey)
            this.rebuildLayout();
      },

      /** Store local layout in REveWindowManager, so it is kept on page reload and shown by all clients */
      sendLayout: function()
      {
         let wm = this.mgr.FindWindowManager();
         if (!wm || !this.localLayout) return;
         let json = JSON.stringify(this.localLayout).replace(/"/g, '\\"');
         this.mgr.SendMIR('SetLayoutFromJson("' + json + '")', wm.fElementId, "ROOT::Experimental::REveWindowManager");
      },

      /** Splitter divider was moved: weights of the pack items follow the new sizes.
       *  Resize of the whole splitter, e.g. with the browser window, keeps the proportions and is ignored. */
      onLayoutSplitterResize: function(splitter, sizes)
      {
         if (!sizes || sizes.length != splitter._eveIdx.length || !sizes.every(s => s > 0)) return;
         let pack = splitter._evePath.reduce((p, i) => p.Items[i].Pack, this.shownLayout);
         let items = splitter._eveIdx.map(i => pack.Items[i]);
         let wsum = items.reduce((a, it) => a + it.Weight, 0), ssum = sizes.reduce((a, s) => a + s, 0);
         if (items.every((it, i) => Math.abs(it.Weight / wsum - sizes[i] / ssum) < 0.01)) return;

         items.forEach((it, i) => it.Weight = wsum * sizes[i] / ssum);
         this.shownLayoutKey = this.layoutKey(this.shownLayout);
         this.localLayout = JSON.parse(JSON.stringify(this.shownLayout));

         // send once the divider stops moving
         clearTimeout(this.layoutResizeTimer);
         this.layoutResizeTimer = setTimeout(() => {
            // relative sizes, so that later resize of the splitter keeps the proportions
            let areas = splitter.getContentAreas();
            if (areas.length == items.length)
               areas.forEach((a, i) => a.getLayoutData().setSize((i == areas.length - 1) ? "auto" :
                                                                (100 * items[i].Weight / wsum).toFixed(2) + "%"));
            this.sendLayout();
         }, 300);
      },

      /** Start local modification of the layout, returns the local layout tree */
      editLayout: function()
      {
         if (!this.localLayout)
            this.localLayout = this.getEffectiveLayout();
         return this.localLayout;
      },

      /** Find leaf with given viewer id, returns { pack, index } */
      findLayoutLeaf: function(pack, id)
      {
         for (let i = 0; i < pack.Items.length; ++i) {
            let it = pack.Items[i];
            if (it.Pack) {
               let res = this.findLayoutLeaf(it.Pack, id);
               if (res) return res;
            } else if (it.ViewerId == id) {
               return { pack, index: i };
            }
         }
         return null;
      },

      /** Remove viewer from the layout tree, empty packs are removed too */
      removeLayoutLeaf: function(pack, id)
      {
         for (let i = 0; i < pack.Items.length; ++i) {
            let it = pack.Items[i];
            if (it.Pack) {
               if (this.removeLayoutLeaf(it.Pack, id)) {
                  if (!it.Pack.Items.length) pack.Items.splice(i, 1);
                  return true;
               }
            } else if (it.ViewerId == id) {
               pack.Items.splice(i, 1);
               return true;
            }
         }
         return false;
      },

      /** Swap positions of two viewers in the layout */
      swapViewers: function(id1, id2)
      {
         let layout = this.editLayout();
         let l1 = this.findLayoutLeaf(layout, id1), l2 = this.findLayoutLeaf(layout, id2);
         if (!l1 || !l2) return;
         l1.pack.Items[l1.index].ViewerId = id2;
         l2.pack.Items[l2.index].ViewerId = id1;
         this.rebuildLayout();
         this.sendLayout();
      },

      /** Move viewer next to the target viewer, side is "left", "right", "top" or "bottom" */
      dockViewer: function(id, targetId, side)
      {
         let layout = this.editLayout();
         this.removeLayoutLeaf(layout, id);
         let tgt = this.findLayoutLeaf(layout, targetId);
         if (!tgt) return;

         let vertical = (side == "top" || side == "bottom");
         let before = (side == "left" || side == "top");
         let titem = tgt.pack.Items[tgt.index];

         if (!!tgt.pack.Vertical == vertical) {
            // pack has required orientation, split weight of the target
            titem.Weight = (titem.Weight > 0 ? titem.Weight : 1) / 2;
            tgt.pack.Items.splice(before ? tgt.index : tgt.index + 1, 0, { Weight: titem.Weight, ViewerId: id });
         } else {
            // replace target with a sub-pack of required orientation
            let items = [{ Weight: 1, ViewerId: titem.ViewerId }];
            items.splice(before ? 0 : 1, 0, { Weight: 1, ViewerId: id });
            tgt.pack.Items[tgt.index] = { Weight: titem.Weight, Pack: { Vertical: vertical, Items: items } };
         }
         this.rebuildLayout();
         this.sendLayout();
      },

      //==============================================================================
      // Drag and drop of viewers, the toolbar of the view is the drag handle
      //==============================================================================

      setupViewDragAndDrop: function(view, viewer)
      {
         let tbar = view.byId("tbar");
         if (!tbar) return;
         tbar.addStyleClass("eveViewDragHandle");
         tbar.setTooltip("Drag to another view: center to swap, edge to dock\nDrag outside of the browser window: separate window");

         tbar.addEventDelegate({ onAfterRendering: () => {
            let dom = tbar.getDomRef();
            dom.draggable = true;
            dom.ondragstart = e => {
               this.dragViewerId = viewer.fElementId;
               e.dataTransfer.effectAllowed = "move";
               // own type, so that other applications do not accept the drop
               e.dataTransfer.setData("application/x-eve-viewer", viewer.fName);
               view.getDomRef()?.classList.add("eveViewDragSource");
               this.trackDragOutsideWindow();
            };
            dom.ondragend = e => {
               view.getDomRef()?.classList.remove("eveViewDragSource");
               delete this.dragViewerId;
               this.showDropIndicator(null);
               let outside = this.dragOutsideWindow || e.clientX < 0 || e.clientY < 0 ||
                             e.clientX > window.innerWidth || e.clientY > window.innerHeight;
               if (e.dataTransfer.dropEffect == "none" && outside)
                  this.openViewerWindow(viewer, e.screenX, e.screenY);
            };
         }});

         view.addEventDelegate({ onAfterRendering: () => {
            let dom = view.getDomRef();
            dom.ondragover = e => {
               let zone = this.getDropZone(e, dom, viewer);
               if (!zone) return;
               e.preventDefault();
               e.dataTransfer.dropEffect = "move";
               this.showDropIndicator(dom.getBoundingClientRect(), zone);
            };
            dom.ondragleave = e => {
               if (!dom.contains(e.relatedTarget)) this.showDropIndicator(null);
            };
            dom.ondrop = e => {
               let zone = this.getDropZone(e, dom, viewer);
               this.showDropIndicator(null);
               if (!zone) return;
               e.preventDefault();
               let id = this.dragViewerId;
               delete this.dragViewerId;
               if (zone == "center")
                  this.swapViewers(id, viewer.fElementId);
               else
                  this.dockViewer(id, viewer.fElementId, zone);
            };
         }});
      },

      /** Keep this.dragOutsideWindow up to date, some browsers report no position at the end of a drag */
      trackDragOutsideWindow: function()
      {
         this.dragOutsideWindow = false;
         if (this.dragWindowListeners) return;
         this.dragWindowListeners = true;
         // leaving the page goes through its edge; relatedTarget alone is not enough, Safari never sets it
         document.addEventListener("dragleave", e => {
            if (!e.relatedTarget && (e.clientX < 5 || e.clientY < 5 || e.clientX > window.innerWidth - 5 ||
                                     e.clientY > window.innerHeight - 5))
               this.dragOutsideWindow = true;
         });
         document.addEventListener("dragenter", () => { this.dragOutsideWindow = false; });
      },

      /** Move viewer to a separate browser window at given screen position, as with the Single=name URL option.
       *  The viewer keeps its place in the layout and comes back there when the window is closed. */
      openViewerWindow: function(viewer, screenX, screenY)
      {
         let r = viewer.ca?.getDomRef()?.getBoundingClientRect();
         let w = Math.round(r?.width || 800), h = Math.round(r?.height || 600);
         // open now, popup blockers allow it only right after a user action, the address comes from the server
         let win = window.open("", "_blank", `popup,width=${w},height=${h},left=${screenX},top=${screenY}`);
         if (!win) {
            MessageToast.show("Allow pop-up windows for this page to show views in separate windows");
            return;
         }

         viewer.undocked = true;
         this.rebuildLayout();
         let timer = setInterval(() => {
            if (!win.closed) return;
            clearInterval(timer);
            delete viewer.undocked;
            this.rebuildLayout();
         }, 500);
         this.mgr.RequestNewWindowUrl(url => {
            if (!url) {
               win.close();
               MessageToast.show("Server does not allow new windows");
               return;
            }
            // address of this page with the new key, the server may know itself under a different host or port
            let srv = new URL(url, window.location.href), u = new URL(window.location.href);
            u.search = srv.search;
            u.hash = srv.hash;
            u.searchParams.set("Single", viewer.fName);
            // the new window got a copy of sessionStorage, with the reload key of this page JSROOT would
            // connect as this page and replace its connection
            try {
               win.sessionStorage.removeItem("RWebWindow_Key");
            } catch (e) {
               console.warn("openViewerWindow: cannot access sessionStorage of the new window", e);
            }
            win.location.href = u.href;
         });
      },

      /** Returns drop zone for current drag position or null if drop is not possible */
      getDropZone: function(e, dom, viewer)
      {
         if (this.dragViewerId === undefined || this.dragViewerId == viewer.fElementId) return null;
         let r = dom.getBoundingClientRect();
         let x = (e.clientX - r.left) / r.width, y = (e.clientY - r.top) / r.height;
         let edges = { left: x, right: 1 - x, top: y, bottom: 1 - y };
         let side = Object.keys(edges).reduce((a, b) => edges[a] <= edges[b] ? a : b);
         return edges[side] < 0.25 ? side : "center";
      },

      showDropIndicator: function(rect, zone)
      {
         if (!this.dropIndicator) {
            this.dropIndicator = document.createElement("div");
            this.dropIndicator.className = "eveViewDropIndicator";
            document.body.appendChild(this.dropIndicator);
         }
         let s = this.dropIndicator.style;
         if (!rect) { s.display = "none"; return; }

         let l = rect.left, t = rect.top, w = rect.width, h = rect.height;
         if (zone == "right") l += w / 2;
         if (zone == "bottom") t += h / 2;
         if (zone == "left" || zone == "right") w /= 2;
         if (zone == "top" || zone == "bottom") h /= 2;
         Object.assign(s, { display: "block", left: l + "px", top: t + "px", width: w + "px", height: h + "px" });
      },

      subscribeView: function(viewer, e)
      {
         let vMenu = this.getView().byId("menuViewId");
         viewer.subscribed = true;
         viewer.pendInstance = true;
         e.getSource().setEnabled(false);

         this.mgr.SendMIR("ConnectClient()", viewer.fElementId, "ROOT::Experimental::REveViewer");
      },

      switchSingle: function (elem, oEvent) {
         let viewer = this.mgr.GetElement(elem.fElementId);
         // console.log('item pressed', item.getText(), elem);

         let name = viewer.fName;
        // if (name.indexOf(" ") > 0) name = name.substr(0, name.indexOf(" "));
         // FIXME: one need better way to deliver parameters to the selected view
         EVE.$eve7tmp = { mgr: this.mgr, eveViewerId: elem.fElementId};

         let oRouter = UIComponent.getRouterFor(this);
         if (name == "Table") {
            oRouter.navTo("Table", { viewName: name });
         }
         else if (name == "Lego")
         {
            oRouter.navTo("Lego", { viewName: name });
         }
         else {
            oRouter.navTo("View", { viewName: name });
         }
      },

      removeView: function(viewer) {
         if (viewer.fRnrSelf) {
            viewer.subscribed = false;
            this.rebuildLayout();
            if (viewer.ca) {
               viewer.ca.destroy();
               delete viewer.ca;
            }

            let vMenu = this.getView().byId("menuViewId");
            vMenu.getItems().forEach(c => { if (c.eveView == viewer) c.setEnabled(true); });

            let siList = viewer.childs;
            for (let i = 0; i < siList.length; ++i)
            {
               let scene = this.mgr.GetElement(siList[i].fSceneId);
               this.mgr.recursiveDestroy(scene);
            }

            let mir = "DisconnectClient()";
            this.mgr.SendMIR(mir, viewer.fElementId, "ROOT::Experimental::REveViewer");
         }
      },

      setToolbarExpandedAction(va) {
         let bar = va.byId("tbar");
         let ca = bar.getContent();
         while (bar.getContent().length > 1)
            bar.removeContent(bar.getContent().length - 1);

         var bb = new sap.m.Button({
            type: MobileLibrary.ButtonType.Default,
            text: "Back",
            enabled: true,
            press: function () {
               window.history.go(-1)
            }
         });
         bar.addContent(bb);
      },

      onEveManagerInit: function() {
         // console.log("manager updated");
         this.UpdateCommandsButtons(this.mgr.commands);
         this.updateViewers();
      },

      /*
       * processWaitingMsg: function() { for ( let i = 0; i <
       * msgToWait.length; ++i ) {
       * this.onWebsocketMsg(handleToWait, msgToWait[i]); }
       * handleToWait = 0; msgToWait = []; },
       */
      event: function() {
         // this._event = lst;

      },

      setMainVerticalSplitterHeight: function(){
         let mainViewHeight = document.body.clientHeight;
         let mainToolbarHeight = 49;
         let height = mainViewHeight - mainToolbarHeight;
         let splitter =  this.getView().byId("MainAreaSplitter");
         if (splitter) {
            // console.log("set splitter height >>> " , height);
            splitter.setHeight(height + "px");
         }
      },

      onAfterRendering: function(){
         let me = this;
         setTimeout(
            function(){
               $(window).on("resize", function() {
                  me.setMainVerticalSplitterHeight();
               });
               me.setMainVerticalSplitterHeight();
            }, 100);
      },

      onToolsMenuAction : function (oEvent) {

         let item = oEvent.getParameter("item");

         switch (item.getText()) {
            case "GED Editor": this.getView().byId("Summary").getController().toggleEditor(); break;
          //  case "Event statusbar": this.toggleShowStatus(); break;
          //  case "Toolbar": this.toggleToolBar(); break;
          //  case "Tooltip info": this.toggleToolTip(); break;
         }
      },

      showHelp : function(oEvent) {
         alert("User support: root-webgui@cern.ch");
      },

      showUserURL : function(oEvent) {
         MobileLibrary.URLHelper.redirect("https://github.com/alja/jsroot/blob/dev/eve7.md", true);
      },

      executeCommand : function(cmd) {
         if (!cmd || !this.mgr.handle)
            return;

         // idealy toolbar shoulf be unactive, but thus is the role af web application
         if (this.mgr.busyProcessingChanges) {
            return;
         }

         this.mgr.SendMIR(cmd.func, cmd.elementid, cmd.elementclass);

         if ((cmd.name == "QuitRoot") && window) {
             window.close();
         }
      }

   });
});
