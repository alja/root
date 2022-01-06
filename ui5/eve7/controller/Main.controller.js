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
         this.initClientLog();

         var conn_handle = Component.getOwnerComponentFor(this.getView()).getComponentData().conn_handle;
         this.mgr.UseConnection(conn_handle);
         // this.mgr.UseConnection(this.getView().getViewData().conn_handle);

         this.mgr.RegisterController(this);
         // method to found summary controller by ID and set manager to it
         var elem = this.byId("Summary");
         var ctrl = elem.getController();
         ctrl.SetMgr(this.mgr);

      },

      onDisconnect : function() {
         var t = this.byId("centerTitle");
         t.setHtmlText("<strong style=\"color: red;\">Client Disconnected !</strong>");
      },

      /** called when relative number of possible send operation below or over the threshold  */
      onSendThresholdChanged: function(below, value) {
         var t = this.byId("centerTitle");
         t.$().css('color', below ? 'yellow' : '')
      },

      initClientLog: function() {
         var consoleObj = {};
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
         console.warning = function ()
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
            var toolbar = pthis.byId("otb1");
            toolbar.addContentRight(logCtrl.getButton());
         });
         consoleObj.alert = true;
         JSROOT.EVE.alert = function (oText)
         {
            if (consoleObj.alert)
            {
               MessageBox.error(oText, {
                  actions: ["Stop Alerts", MessageBox.Action.CLOSE],
                  onClose: function (sAction)
                  {
                     if (sAction == "Stop Alerts") { consoleObj.alert = false; }
                  }
               });
            }
         };
      },

      UpdateCommandsButtons: function(cmds) {
         if (!cmds || this.commands) return;

         var toolbar = this.byId("otb1");

         this.commands = cmds;
         for (var k = cmds.length-1; k>=0; --k) {
            var btn = new mButton({
               icon: cmds[k].icon,
               text: cmds[k].name,
               press: this.executeCommand.bind(this, cmds[k])
            });
            toolbar.insertContentLeft(btn, 0);
         }
      },

      viewItemPressed: function (elem, oEvent) {
         var item = oEvent.getSource();
         // console.log('item pressed', item.getText(), elem);

         var name = item.getText();
         if (name.indexOf(" ") > 0) name = name.substr(0, name.indexOf(" "));
         // FIXME: one need better way to deliver parameters to the selected view
         JSROOT.$eve7tmp = { mgr: this.mgr, eveViewerId: elem.fElementId};

         var oRouter = UIComponent.getRouterFor(this);
         if (name == "Table")
            oRouter.navTo("Table", { viewName: name });
         else if (name == "Lego")
            oRouter.navTo("Lego", { viewName: name });
         else
            oRouter.navTo("View", { viewName: name });
      },

      updateViewers: function(loading_done) {
         var viewers = this.mgr.FindViewers();

         // first check number of views to create
         var staged = [];
         for (var n=0;n<viewers.length;++n) {
            var el = viewers[n];
            if (!el.$view_created && el.fRnrSelf) staged.push(el);
         }
         if (staged.length == 0) return;

         // console.log("FOUND viewers", viewers.length, "not yet exists", staged.length);

         if (staged.length > 1) {
            var vMenu = this.getView().byId("menuViewId");
            var item = new mMenuItem({text:"Browse to"});
            vMenu.addItem(item);
            for (var n=0;n<staged.length;++n)
               item.addItem(new mMenuItem({text: staged[n].fName, press: this.viewItemPressed.bind(this, staged[n]) }));
         }

         var main = this, vv = null, sv = this.getView().byId("MainAreaSplitter");

         for (var n=0;n<staged.length;++n) {
            var elem = staged[n];
            var viewid = "EveViewer" + elem.fElementId;

            // create missing view
            elem.$view_created = true;
            console.log("Creating view", viewid);

            var oLd = undefined;
            if ((n == 0) && (staged.length > 1))
               oLd = new SplitterLayoutData({ resizable: true, size: "50%" });

            var vtype = "rootui5.eve7.view.GL";
            if (elem.fName === "Table")
               vtype = "rootui5.eve7.view.EveTable"; // AMT temporary solution
            else if (elem.fName === "Lego")
               vtype = "rootui5.eve7.view.Lego"; // AMT temporary solution

            var oOwnerComponent = Component.getOwnerComponentFor(this.getView());
            var view = oOwnerComponent.runAsOwner(function() {
               return new sap.ui.xmlview({
                  id: viewid,
                  viewName: vtype,
                  viewData: { mgr: main.mgr, eveViewerId: elem.fElementId },
                  layoutData: oLd
               });
            });

            if (n == 0) {
               sv.addContentArea(view);
               continue;
            }

            if (!vv) {
               vv = new Splitter("SecondaryViewSplitter", { orientation : "Vertical" });
               sv.addContentArea(vv);
            }

            vv.addContentArea(view);
         }
      },

      onEveManagerInit: function() {
         // console.log("manager updated");
         this.UpdateCommandsButtons(this.mgr.commands);
         this.updateViewers();
      },

      /*
       * processWaitingMsg: function() { for ( var i = 0; i <
       * msgToWait.length; ++i ) {
       * this.onWebsocketMsg(handleToWait, msgToWait[i]); }
       * handleToWait = 0; msgToWait = []; },
       */
      event: function() {
         // this._event = lst;

      },

      setMainVerticalSplitterHeight: function(){
         var mainViewHeight = document.body.clientHeight;
         var mainToolbarHeight = 49;
         var height = mainViewHeight - mainToolbarHeight;
         var splitter =  this.getView().byId("MainAreaSplitter");
         if (splitter) {
            // console.log("set splitter height >>> " , height);
            splitter.setHeight(height + "px");
         }
      },

      onAfterRendering: function(){
         var me = this;
         setTimeout(
            function(){
               $(window).on("resize", function() {
                  me.setMainVerticalSplitterHeight();
               });
               me.setMainVerticalSplitterHeight();
            }, 100);
      },

      onToolsMenuAction : function (oEvent) {

         var item = oEvent.getParameter("item");

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
