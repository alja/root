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
               'sap/ui/model/json/JSONModel'
], function(Component, UIComponent, Controller, Splitter, SplitterLayoutData, MobileLibrary, mButton, mMenuItem, MessageBox, EveManager, XMLView, JSONModel) {

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
         this.localLayout = null;    // layout modified by user (swap, drag and drop), null - use server layout
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

      /** Viewer is shown when it is subscribed and its view was created */
      isViewerShown: function(viewer)
      {
         return viewer && viewer.subscribed && viewer.ca && viewer.fRnrSelf;
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

      /** Creates control for a layout item, returns null when there is nothing to show */
      buildLayoutItem: function(item, shown)
      {
         if (!item.Pack) {
            let viewer = this.mgr.GetElement(item.ViewerId);
            if (!this.isViewerShown(viewer)) return null;
            shown.push(viewer);
            return viewer.ca;
         }

         let parts = [];
         item.Pack.Items.forEach(it => {
            let ctrl = this.buildLayoutItem(it, shown);
            if (ctrl) parts.push({ ctrl, weight: it.Weight > 0 ? it.Weight : 1 });
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

         this.shownViewers = [];
         let root = this.buildLayoutItem({ Pack: this.getEffectiveLayout() }, this.shownViewers);
         if (root) {
            root.setLayoutData(new SplitterLayoutData({ resizable: true, size: "auto" }));
            this.primarySplitter.addContentArea(root);
         }

         this.shownViewers.forEach((v, i) => {
            let sBtn = v.ca.byId("tbar").getContent()[2];
            sBtn.setIcon("sap-icon://" + (i == 0 ? "arrow-right" : "arrow-left"));
            sBtn.setEnabled(this.shownViewers.length > 1);
         });
      },

      onWindowLayoutChanged: function()
      {
         this.localLayout = null;
         this.rebuildLayout();
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
      },

      //==============================================================================
      // Drag and drop of viewers, the toolbar of the view is the drag handle
      //==============================================================================

      setupViewDragAndDrop: function(view, viewer)
      {
         let tbar = view.byId("tbar");
         if (!tbar) return;
         tbar.addStyleClass("eveViewDragHandle");
         tbar.setTooltip("Drag to another view: center to swap, edge to dock");

         tbar.addEventDelegate({ onAfterRendering: () => {
            let dom = tbar.getDomRef();
            dom.draggable = true;
            dom.ondragstart = e => {
               this.dragViewerId = viewer.fElementId;
               e.dataTransfer.effectAllowed = "move";
               e.dataTransfer.setData("text/plain", viewer.fName);
            };
            dom.ondragend = () => {
               delete this.dragViewerId;
               this.showDropIndicator(null);
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

      /** Swap viewer with the first shown viewer, the first one is swapped with the second */
      switchViewSides: function (viewer) {
         let shown = this.shownViewers || [];
         if (shown.length < 2) return;
         let other = (shown[0] === viewer) ? shown[1] : shown[0];
         this.swapViewers(viewer.fElementId, other.fElementId);
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
