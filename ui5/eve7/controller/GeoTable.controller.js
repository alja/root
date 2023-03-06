sap.ui.define([
    'sap/ui/core/mvc/Controller',
    'sap/ui/table/Column',
    'sap/m/Text',
    "sap/ui/core/ResizeHandler",
    'sap/ui/core/UIComponent',
    'rootui5/geom/model/GeomBrowserModel',
    'rootui5/geom/lib/ColorBox'
], function (Controller, tableColumn, mText, ResizeHandler, UIComponent,GeomBrowserModel,GeomColorBox) {

    "use strict";

    return Controller.extend("rootui5.eve7.controller.GeoTable", {

        onInit: function () {
            // disable narrowing axis range
            EVE.JSR.settings.Zooming = false;

            let data = this.getView().getViewData();
            if (data) {
                this.setupManagerAndViewType(data.eveViewerId, data.mgr);
            }
            else {
                UIComponent.getRouterFor(this).getRoute("GeoTable").attachPatternMatched(this.onViewObjectMatched, this);
            }
            // this.model = new GeomBrowserModel();
            // this.model.useIndexSuffix = false;


        //    ResizeHandler.register(this.getView(), this.onResize.bind(this));
        },

        onViewObjectMatched: function (oEvent) {
            let args = oEvent.getParameter("arguments");
            this.setupManagerAndViewType(EVE.$eve7tmp.eveViewerId, EVE.$eve7tmp.mgr);
            delete EVE.$eve7tmp;
        },

        setupManagerAndViewType: function (eveViewerId, mgr) {
            this.eveViewerId = eveViewerId;
            this.mgr       = mgr;

            let eviewer = this.mgr.GetElement(this.eveViewerId);
            let sceneInfo = eviewer.childs[0];
            let scene = this.mgr.GetElement(sceneInfo.fSceneId);
            let topNodeEve = scene.childs[0];

            // let obj = topNodeEve.objDesc;

            // let topNode = this.model.buildTree(obj.nodes);

            //console.log("geoTable top node ", topNode);

            //this.geoTable.setNoData("");
            //this.geoTable.setShowNoData(false);

            //this.model.setFullModel(topNode);
            //this.model.refresh(true);

            let h = this.byId('geomHierarchyPanel');

            let websocket = this.mgr.handle.createChannel();

            h.getController().configure({
               websocket,
               // viewer: this,
               // standalone: this.standalone,
               show_columns: true,
               jsroot: EVE.JSR
            });

            console.log('channel id is', websocket.getChannelId());
            //let fcall = "SetChannel(" + websocket.getChannelId() + ")";

            //this.mgr.SendMIR(fcall, topNodeEve.fElementId, topNodeEve._typename);
            this.mgr.handle.send("SETCHANNEL:" + topNodeEve.fElementId + "," + websocket.getChannelId());
        },
        /*
        onResize : function() {
        },
        controllerIsMapped() {
            let ev = this.mgr.GetElement(this.eveViewerId);
            return ev.fRnrSelf;
        },
        onResizeTimeout: function () {
            delete this.resize_tmout;
        },
        onSceneCreate: function (element, id) {
            // console.log("LEGO onSceneCreate", id);
        },

        sceneElementChange: function (el) {
            //console.log("LEGO element changed");
        },

        endChanges: function (oEvent) {
            if (!this.controllerIsMapped()) return;

            let domref = this.byId("legoPlotPlace").getDomRef();
            this.canvas_json = EVE.JSR.parse( atob(this.eve_lego.fTitle) );
            EVE.JSR.redraw(domref, this.canvas_json);
        },

        elementRemoved: function (elId) {
        },

        SelectElement: function (selection_obj, element_id, sec_idcs) {
        },

        UnselectElement: function (selection_obj, element_id) {
        },
        */
        switchSingle: function () {
            let oRouter = UIComponent.getRouterFor(this);
            EVE.$eve7tmp = { mgr: this.mgr, eveViewerId: this.eveViewerId };

            oRouter.navTo("GeoTable", { viewName: this.mgr.GetElement(this.eveViewerId).fName });
        },

        swap: function () {
            this.mgr.controllers[0].switchViewSides(this.mgr.GetElement(this.eveViewerId));
        },

        detachViewer: function () {
            this.mgr.controllers[0].removeView(this.mgr.GetElement(this.eveViewerId));
            this.destroy();
        }
    });
});
