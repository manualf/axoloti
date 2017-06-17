package axoloti;

import axoloti.chunks.ChunkData;
import axoloti.chunks.ChunkParser;
import axoloti.chunks.Cpatch_display;
import axoloti.chunks.FourCC;
import axoloti.chunks.FourCCs;
import axoloti.datatypes.DataType;
import axoloti.inlets.IInletInstanceView;
import axoloti.iolet.IoletAbstract;
import axoloti.mvc.AbstractController;
import axoloti.mvc.AbstractDocumentRoot;
import axoloti.mvc.array.ArrayModel;
import axoloti.mvc.array.ArrayView;
import axoloti.object.AxoObjectAbstract;
import axoloti.object.AxoObjectFromPatch;
import axoloti.object.AxoObjectInstanceAbstract;
import axoloti.object.ObjectInstanceController;
import axoloti.objectviews.AxoObjectInstanceViewAbstract;
import axoloti.objectviews.AxoObjectInstanceViewFactory;
import axoloti.objectviews.IAxoObjectInstanceView;
import axoloti.outlets.IOutletInstanceView;
import axoloti.parameters.ParameterInstance;
import axoloti.parameterviews.IParameterInstanceView;
import java.awt.Dimension;
import java.awt.Point;
import java.awt.datatransfer.DataFlavor;
import java.awt.datatransfer.Transferable;
import java.awt.datatransfer.UnsupportedFlavorException;
import java.awt.dnd.DnDConstants;
import java.awt.dnd.DropTarget;
import java.awt.dnd.DropTargetDragEvent;
import java.awt.dnd.DropTargetDropEvent;
import java.awt.event.FocusEvent;
import java.awt.event.FocusListener;
import java.beans.PropertyChangeEvent;
import java.io.File;
import java.io.IOException;
import java.io.InputStream;
import java.nio.ByteBuffer;
import java.util.ArrayList;
import java.util.HashMap;
import java.util.List;
import java.util.Map;
import java.util.logging.Level;
import java.util.logging.Logger;
import javax.swing.JFrame;
import org.simpleframework.xml.Serializer;
import org.simpleframework.xml.convert.AnnotationStrategy;
import org.simpleframework.xml.core.Persister;
import org.simpleframework.xml.strategy.Strategy;
import qcmds.QCmdChangeWorkingDirectory;
import qcmds.QCmdCompileModule;
import qcmds.QCmdCompilePatch;
import qcmds.QCmdCreateDirectory;
import qcmds.QCmdLock;
import qcmds.QCmdMemRead;
import qcmds.QCmdProcessor;
import qcmds.QCmdStart;
import qcmds.QCmdStop;
import qcmds.QCmdUploadPatch;

public abstract class PatchView extends PatchAbstractView {

    // shortcut patch names
    final static String patchComment = "patch/comment";
    final static String patchInlet = "patch/inlet";
    final static String patchOutlet = "patch/outlet";
    final static String patchAudio = "audio/";
    final static String patchAudioOut = "audio/out stereo";
    final static String patchMidi = "midi";
    final static String patchMidiKey = "midi/in/keyb";
    final static String patchDisplay = "disp/";

    ArrayView<IAxoObjectInstanceView> objectInstanceViews;
    ArrayView<INetView> netViews;
    
    PatchView(PatchController patchController) {
        super(patchController);
    }

    public void PostConstructor() {
        objectInstanceViews = new ArrayView<IAxoObjectInstanceView>(controller.objectInstanceControllers) {
            @Override
            public IAxoObjectInstanceView viewFactory(AbstractController ctrl) {
                IAxoObjectInstanceView view = AxoObjectInstanceViewFactory.createView((ObjectInstanceController) ctrl, (PatchViewSwing) PatchView.this);
                view.PostConstructor();
                return view;
            }

            @Override
            public void updateUI() {
                removeAllObjectViews();
                for (IAxoObjectInstanceView v : getSubViews()) {
                    add(v);
                }
            }

            @Override
            public void removeView(IAxoObjectInstanceView view) {
            }
        };
        controller.objectInstanceControllers.addView(objectInstanceViews);

        netViews = new ArrayView<INetView>(controller.netControllers) {
            @Override
            public INetView viewFactory(AbstractController ctrl) {
                INetView view = new NetView((Net) (ctrl.getModel()), (NetController) ctrl, (PatchViewSwing) PatchView.this);
                view.PostConstructor();
                return view;
            }

            @Override
            public void updateUI() {
                removeAllNetViews();
                for (INetView v : getSubViews()) {
                    add(v);
                }
            }

            @Override
            public void removeView(INetView view) {
            }
        };
        controller.netControllers.addView(netViews);
    }

    public ArrayView<IAxoObjectInstanceView> getObjectInstanceViews() {
        return objectInstanceViews;
    }

    public abstract PatchViewportView getViewportView();

    public void initViewportView() {
    }

    public abstract void repaint();

    public abstract Point getLocationOnScreen();

    public abstract void requestFocus();

    public abstract void AdjustSize();

    abstract void setCordsInBackground(boolean cordsInBackground);

    void paste(String v, Point pos, boolean restoreConnectionsToExternalOutlets) {
        SelectNone();
        getController().paste(v, pos, restoreConnectionsToExternalOutlets);
    }

    public void setFileNamePath(String FileNamePath) {
        getController().setFileNamePath(FileNamePath);
    }

    TextEditor NotesFrame;

    void ShowNotesFrame() {
        if (NotesFrame == null) {
            NotesFrame = new TextEditor(new StringRef(), getPatchFrame());
            NotesFrame.setTitle("notes");
            NotesFrame.SetText(getController().getModel().notes);
            NotesFrame.addFocusListener(new FocusListener() {
                @Override
                public void focusGained(FocusEvent e) {
                }

                @Override
                public void focusLost(FocusEvent e) {
                    getController().getModel().notes = NotesFrame.GetText();
                }
            });
        }
        NotesFrame.setVisible(true);
        NotesFrame.toFront();
    }

    public ObjectSearchFrame osf;

    public void ShowClassSelector(Point p, AxoObjectInstanceViewAbstract o, String searchString) {
        if (isLocked()) {
            return;
        }
        if (osf == null) {
            osf = new ObjectSearchFrame(getController());
        }
        osf.Launch(p, o, searchString);
    }

    private Map<DataType, Boolean> cableTypeEnabled = new HashMap<DataType, Boolean>();

    public void setCableTypeEnabled(DataType type, boolean enabled) {
        cableTypeEnabled.put(type, enabled);
    }

    public Boolean isCableTypeEnabled(DataType type) {
        if (cableTypeEnabled.containsKey(type)) {
            return cableTypeEnabled.get(type);
        } else {
            return true;
        }
    }

    public AxoObjectFromPatch ObjEditor;

    public void setObjEditor(AxoObjectFromPatch ObjEditor) {
        this.ObjEditor = ObjEditor;
    }

    public void ShowCompileFail() {
        setLocked(false);
    }

    public abstract void add(IAxoObjectInstanceView v);

    public abstract void remove(IAxoObjectInstanceView v);

    public abstract void removeAllObjectViews();

    public abstract void removeAllNetViews();

    public abstract void add(INetView v);

    void SelectAll() {
        for (IAxoObjectInstanceView o : objectInstanceViews) {
            o.setSelected(true);
        }
    }

    public void SelectNone() {
        for (IAxoObjectInstanceView o : objectInstanceViews) {
            o.setSelected(false);
        }
    }

    PatchModel getSelectedObjects() {
        PatchModel p = new PatchModel();
        for (IAxoObjectInstanceView o : getObjectInstanceViews()) {
            if (o.isSelected()) {
                p.objectinstances.add(o.getModel());
            }
        }
        p.nets = new ArrayModel<Net>();
        for (INetView n : netViews) {
            int sel = 0;
            for (IInletInstanceView i : n.getDestinationViews()) {
                if (i.getObjectInstanceView().isSelected()) {
                    sel++;
                }
            }
            for (IOutletInstanceView i : n.getSourceViews()) {
                if (i.getObjectInstanceView().isSelected()) {
                    sel++;
                }
            }
            if (sel > 0) {
                p.nets.add(n.getNet());
            }
        }
        return p;
    }

    enum Direction {
        UP, LEFT, DOWN, RIGHT
    }

    void MoveSelectedAxoObjInstances(Direction dir, int xsteps, int ysteps) {
        if (!isLocked()) {
            int xgrid = 1;
            int ygrid = 1;
            int xstep = 0;
            int ystep = 0;
            switch (dir) {
                case DOWN:
                    ystep = ysteps;
                    ygrid = ysteps;
                    break;
                case UP:
                    ystep = -ysteps;
                    ygrid = ysteps;
                    break;
                case LEFT:
                    xstep = -xsteps;
                    xgrid = xsteps;
                    break;
                case RIGHT:
                    xstep = xsteps;
                    xgrid = xsteps;
                    break;
            }
            boolean isUpdate = false;
            for (IAxoObjectInstanceView o : objectInstanceViews) {
                if (o.isSelected()) {
                    isUpdate = true;
                    Point p = o.getLocation();
                    p.x = p.x + xstep;
                    p.y = p.y + ystep;
                    p.x = xgrid * (p.x / xgrid);
                    p.y = ygrid * (p.y / ygrid);
                    ((ObjectInstanceController)o.getController()).changeLocation(p.x, p.y);
                }
            }
            if (isUpdate) {
                AdjustSize();
            }
        } else {
            Logger.getLogger(PatchView.class.getName()).log(Level.INFO, "can't move: locked");
        }
    }

    void GoLive() {

        QCmdProcessor qCmdProcessor = getController().GetQCmdProcessor();

        qCmdProcessor.AppendToQueue(new QCmdStop());
        if (CConnection.GetConnection().GetSDCardPresent()) {

            String f = "/" + getController().getSDCardPath();
            //System.out.println("pathf" + f);
            if (SDCardInfo.getInstance().find(f) == null) {
                qCmdProcessor.AppendToQueue(new QCmdCreateDirectory(f));
            }
            qCmdProcessor.AppendToQueue(new QCmdChangeWorkingDirectory(f));
            getController().UploadDependentFiles(f);
        } else {
            // issue warning when there are dependent files
            ArrayList<SDFileReference> files = getController().getModel().GetDependendSDFiles();
            if (files.size() > 0) {
                Logger.getLogger(PatchView.class.getName()).log(Level.SEVERE, "Patch requires file {0} on SDCard, but no SDCard mounted", files.get(0).targetPath);
            }
        }
        getController().ShowPreset(0);
        getController().setPresetUpdatePending(false);
        for (AxoObjectInstanceAbstract o : getController().getModel().getObjectInstances()) {
            for (ParameterInstance pi : o.getParameterInstances()) {
                pi.ClearNeedsTransmit();
            }
        }
        getController().WriteCode();
        qCmdProcessor.setPatchController(null);
        for(String module : getController().getModel().getModules()) {
           qCmdProcessor.AppendToQueue(
                   new QCmdCompileModule(getController(),
                           module, 
                           getController().getModel().getModuleDir(module)));
        }
        qCmdProcessor.AppendToQueue(new QCmdCompilePatch(getController()));
        qCmdProcessor.AppendToQueue(new QCmdUploadPatch());
        qCmdProcessor.AppendToQueue(new QCmdStart(getController()));
        qCmdProcessor.AppendToQueue(new QCmdLock(getController()));
        qCmdProcessor.AppendToQueue(new QCmdMemRead(CConnection.GetConnection().getTargetProfile().getPatchAddr(), 8, new IConnection.MemReadHandler() {
            @Override
            public void Done(ByteBuffer mem) {
                int signature = mem.getInt();
                int rootchunk_addr = mem.getInt();

                qCmdProcessor.AppendToQueue(new QCmdMemRead(rootchunk_addr, 8, new IConnection.MemReadHandler() {
                    @Override
                    public void Done(ByteBuffer mem) {
                        int fourcc = mem.getInt();
                        int length = mem.getInt();
                        System.out.println("rootchunk " + FourCC.Format(fourcc) + " len = " + length);

                        qCmdProcessor.AppendToQueue(new QCmdMemRead(rootchunk_addr, length + 8, new IConnection.MemReadHandler() {
                            @Override
                            public void Done(ByteBuffer mem) {
                                ChunkParser cp = new ChunkParser(mem);
                                ChunkData cd = cp.GetOne(FourCCs.PATCH_DISPLAY);
                                if (cd != null) {
                                    Cpatch_display cpatch_display = new Cpatch_display(cd);
                                    CConnection.GetConnection().setDisplayAddr(cpatch_display.pDisplayVector, cpatch_display.nDisplayVector);
                                }
                            }
                        }));
                    }
                }));
            }
        }));
    }

    Dimension GetInitialSize() {
        int mx = 100; // min size
        int my = 100;
        if (objectInstanceViews != null) {
            for (IAxoObjectInstanceView i : objectInstanceViews) {

                Dimension s = i.getPreferredSize();

                int ox = i.getLocation().x + (int) s.getWidth();
                int oy = i.getLocation().y + (int) s.getHeight();

                if (ox > mx) {
                    mx = ox;
                }
                if (oy > my) {
                    my = oy;
                }
            }
        }
        // adding more, as getPreferredSize is not returning true dimension of
        // object
        return new Dimension(mx + 300, my + 300);
    }

    void PreSerialize() {
        if (NotesFrame != null) {
            getController().getModel().notes = NotesFrame.GetText();
        }
        getController().getModel().windowPos = getPatchFrame().getBounds();
    }

    boolean save(File f) {
        PreSerialize();
        boolean b = getController().getModel().save(f);
        if (ObjEditor != null) {
            ObjEditor.UpdateObject();
        }
        return b;
    }
    
    public static PatchFrame OpenPatchModel(PatchModel pm, String fileNamePath) {
        if (pm.getFileNamePath() == null) {
            pm.setFileNamePath("untitled");
        }
        AbstractDocumentRoot documentRoot = new AbstractDocumentRoot();
        PatchController patchController = new PatchController(pm, documentRoot, null);
        PatchFrame pf = new PatchFrame(patchController, QCmdProcessor.getQCmdProcessor());
        patchController.addView(pf);
        return pf;
    }

    public static void OpenPatch(String name, InputStream stream) {
        Strategy strategy = new AnnotationStrategy();
        Serializer serializer = new Persister(strategy);
        try {
            PatchModel patchModel = serializer.read(PatchModel.class, stream);
            PatchFrame pf = OpenPatchModel(patchModel, name);
            pf.setVisible(true);

        } catch (Exception ex) {
            Logger.getLogger(MainFrame.class.getName()).log(Level.SEVERE, null, ex);
        }
    }

    public static PatchFrame OpenPatchInvisible(File f) {
        for (DocumentWindow dw : DocumentWindowList.GetList()) {
            if (f.equals(dw.getFile())) {
                JFrame frame1 = dw.GetFrame();
                if (frame1 instanceof PatchFrame) {
                    return (PatchFrame) frame1;
                } else {
                    return null;
                }
            }
        }

        Strategy strategy = new AnnotationStrategy();
        Serializer serializer = new Persister(strategy);
        try {
            PatchModel patchModel = serializer.read(PatchModel.class, f);
            PatchFrame pf = OpenPatchModel(patchModel, f.getAbsolutePath());
            return pf;
        } catch (java.lang.reflect.InvocationTargetException ite) {
            if (ite.getTargetException() instanceof PatchModel.PatchVersionException) {
                PatchModel.PatchVersionException pve = (PatchModel.PatchVersionException) ite.getTargetException();
                Logger.getLogger(MainFrame.class.getName()).log(Level.SEVERE, "Patch produced with newer version of Axoloti {0} {1}",
                        new Object[]{f.getAbsoluteFile(), pve.getMessage()});
            } else {
                Logger.getLogger(MainFrame.class.getName()).log(Level.SEVERE, null, ite);
            }
            return null;
        } catch (Exception ex) {
            Logger.getLogger(MainFrame.class.getName()).log(Level.SEVERE, null, ex);
            return null;
        }
    }

    public static PatchFrame OpenPatch(File f) {
        PatchFrame pf = OpenPatchInvisible(f);
        pf.setVisible(true);
        pf.setState(java.awt.Frame.NORMAL);
        pf.toFront();
        return pf;
    }

    @Deprecated
    public void updateNetVisibility() {
        for (INetView n : netViews) {
            DataType d = n.getNet().getDataType();
            if (d != null) {
                n.setVisible(isCableTypeEnabled(d));
            }
        }
        repaint();
    }

    public void Close() {
        setLocked(false);
        /*
        IAxoObjectInstanceView c[] = (IAxoObjectInstanceView[])objectInstanceViews.getSubViews().toArray();
        for (IAxoObjectInstanceView o : c) {
            o.getModel().Close();
        }*/
        if (NotesFrame != null) {
            NotesFrame.dispose();
        }
        if ((getController().getSettings() != null)
                && (getController().getSettings().editor != null)) {
            getController().getSettings().editor.dispose();
        }
    }

    public Dimension GetSize() {
        int nx = 0;
        int ny = 0;
        // negative coordinates?
        for (IAxoObjectInstanceView o : objectInstanceViews) {
            Point p = o.getLocation();
            if (p.x < nx) {
                nx = p.x;
            }
            if (p.y < ny) {
                ny = p.y;
            }
        }
        if ((nx < 0) || (ny < 0)) { // move all to positive coordinates
            for (IAxoObjectInstanceView o : objectInstanceViews) {
                Point p = o.getLocation();
                // FIXME
                // o.SetLocation(p.x - nx, p.y - ny);
            }
        }

        int mx = 0;
        int my = 0;
        for (IAxoObjectInstanceView o : objectInstanceViews) {
            Point p = o.getLocation();
            Dimension s = o.getSize();
            int px = p.x + s.width;
            int py = p.y + s.height;
            if (px > mx) {
                mx = px;
            }
            if (py > my) {
                my = py;
            }
        }
        return new Dimension(mx, my);
    }

    IAxoObjectInstanceView getObjectInstanceView(AxoObjectInstanceAbstract o) {
        return objectInstanceViews.getViewOfModel(o);
    }

    void deleteSelectedAxoObjectInstanceViews() {
        Logger.getLogger(PatchModel.class.getName()).log(Level.INFO, "deleteSelectedAxoObjInstances()");
        if (!isLocked()) {
            boolean succeeded = false;
            IAxoObjectInstanceView oiv[] = objectInstanceViews.getSubViews().toArray(new IAxoObjectInstanceView[0]);
            for (IAxoObjectInstanceView o : oiv) {
                if (o.isSelected()) {
                    succeeded |= getController().delete((ObjectInstanceController)o.getController());
                }
            }
        } else {
            Logger.getLogger(PatchModel.class.getName()).log(Level.INFO, "Can't delete: locked!");
        }
    }

    public INetView GetNetView(IInletInstanceView i) {
        for (INetView netView : netViews) {
            for (IInletInstanceView d : netView.getDestinationViews()) {
                if (d == i) {
                    return netView;
                }
            }
        }
        return null;
    }

    public INetView GetNetView(IOutletInstanceView o) {
        for (INetView netView : netViews) {
            for (IOutletInstanceView d : netView.getSourceViews()) {
                if (d == o) {
                    return netView;
                }
            }
        }
        return null;
    }

    public INetView GetNetView(IoletAbstract io) {
        for (INetView netView : netViews) {
            for (IInletInstanceView d : netView.getDestinationViews()) {
                if (d == io) {
                    return netView;
                }
            }
            for (IOutletInstanceView d : netView.getSourceViews()) {
                if (d == io) {
                    return netView;
                }
            }
        }
        return null;
    }

    public ArrayView<INetView> getNetViews() {
        return netViews;
    }

    public boolean isLocked() {
        return getController().isLocked();
    }

    public void setLocked(boolean locked) {
        getController().setLocked(locked);
    }

    public void ShowPreset(int i) {
        // TODO: reconstruct preset logic
        /*
        ArrayList<IAxoObjectInstanceView> objectInstanceViewsClone = (ArrayList<IAxoObjectInstanceView>) objectInstanceViews.clone();
        for (IAxoObjectInstanceView o : objectInstanceViewsClone) {
            for (IParameterInstanceView p : o.getParameterInstanceViews()) {
                p.ShowPreset(i);
            }
        }
        */
    }

    Map<ParameterInstance, IParameterInstanceView> parameterInstanceViews = new HashMap<>();

    public void updateParameterView(ParameterInstance pi) {
        parameterInstanceViews.get(pi).updateV();
    }

    public abstract void validate();

    public abstract void validateObjects();

    public abstract void validateNets();

    public void modelChanged() {
        modelChanged(true);
    }

    @Override
    public void modelPropertyChange(PropertyChangeEvent evt) {
        if (evt.getPropertyName().equals(PatchController.PATCH_LOCKED)) {
            if ((Boolean)evt.getNewValue() == false) {
                for (IAxoObjectInstanceView o : objectInstanceViews) {
                    o.Unlock();
                }
            } else {
                for (IAxoObjectInstanceView o : objectInstanceViews) {
                    o.Lock();
                }
            }
        } else if (evt.getPropertyName().equals(PatchController.PATCH_OBJECTINSTANCES)) {
            objectInstanceViews.updateUI();
        } else if (evt.getPropertyName().equals(PatchController.PATCH_NETS)) {
            netViews.updateUI();
        }
    }

    public void modelChanged(boolean updateSelection) {
    }

    DropTarget dt = new DropTarget() {

        @Override
        public synchronized void dragOver(DropTargetDragEvent dtde) {
        }

        @Override
        public synchronized void drop(DropTargetDropEvent dtde) {
            Transferable t = dtde.getTransferable();
            if (t.isDataFlavorSupported(DataFlavor.javaFileListFlavor)) {
                try {
                    dtde.acceptDrop(DnDConstants.ACTION_COPY_OR_MOVE);
                    List<File> flist = (List<File>) t.getTransferData(DataFlavor.javaFileListFlavor);
                    for (File f : flist) {
                        if (f.exists() && f.canRead()) {
                            AxoObjectAbstract o = new AxoObjectFromPatch(f);
                            String fn = f.getCanonicalPath();
                            if (getController().GetCurrentWorkingDirectory() != null
                                    && fn.startsWith(getController().GetCurrentWorkingDirectory())) {
                                o.createdFromRelativePath = true;
                            }

                            getController().AddObjectInstance(o, dtde.getLocation());
                        }
                    }
                    dtde.dropComplete(true);
                } catch (UnsupportedFlavorException ex) {
                    Logger.getLogger(PatchViewSwing.class.getName()).log(Level.SEVERE, null, ex);
                } catch (IOException ex) {
                    Logger.getLogger(PatchViewSwing.class.getName()).log(Level.SEVERE, null, ex);
                }
                return;
            }
            super.drop(dtde);
        }
    };
}
