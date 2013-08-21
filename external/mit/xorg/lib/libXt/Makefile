#	$NetBSD: Makefile,v 1.5 2011/08/11 23:15:39 joerg Exp $

.include <bsd.own.mk>

LIB=	Xt
.PATH:	${X11SRCDIR.${LIB}}/include/X11
.PATH:	${X11SRCDIR.${LIB}}/src
.PATH:	${X11SRCDIR.${LIB}}/man
.PATH:	${X11SRCDIR.${LIB}}

SRCS=	ActionHook.c Alloc.c ArgList.c Callback.c ClickTime.c \
	Composite.c Constraint.c Convert.c Converters.c Core.c \
	Create.c Destroy.c Display.c Error.c Event.c EventUtil.c \
	Functions.c GCManager.c Geometry.c GetActKey.c GetResList.c \
	GetValues.c HookObj.c Hooks.c Initialize.c Intrinsic.c \
	Keyboard.c Manage.c NextEvent.c Object.c PassivGrab.c \
	Pointer.c Popup.c PopupCB.c RectObj.c ResConfig.c Resources.c \
	Selection.c SetSens.c SetValues.c SetWMCW.c Shell.c \
	StringDefs.c TMaction.c TMgrab.c TMkey.c TMparse.c TMprint.c \
	TMstate.c Threads.c VarCreate.c VarGet.c Varargs.c Vendor.c

INCS=	CallbackI.h Composite.h CompositeP.h ConstrainP.h Constraint.h \
	ConvertI.h Core.h CoreP.h CreateI.h EventI.h HookObjI.h InitialI.h \
	Intrinsic.h IntrinsicI.h IntrinsicP.h Object.h ObjectP.h \
	PassivGraI.h RectObj.h RectObjP.h ResConfigP.h ResourceI.h \
	SelectionI.h Shell.h ShellI.h ShellP.h StringDefs.h ThreadsI.h \
	TranslateI.h VarargsI.h Vendor.h VendorP.h Xtos.h
INCSDIR=${X11INCDIR}/X11

CPPFLAGS+=	${X11FLAGS.THREADLIB}
CPPFLAGS+=	-I${DESTDIR}${X11INCDIR}/X11
.if defined(__MINIX)
CPPFLAGS+=	-DUSE_SNPRINTF -DLIBXT -I.
.else
CPPFLAGS+=	-DUSE_POLL -DUSE_SNPRINTF -DLIBXT -I.
.endif #  defined(__MINIX)

CPPFLAGS.Intrinsic.c=	-DXFILESEARCHPATHDEFAULT=\"${X11LIBDIR}/%L/%T/%N%C%S:${X11LIBDIR}/%l/%T/%N%C%S:${X11LIBDIR}/%T/%N%C%S:${X11LIBDIR}/%L/%T/%N%S:${X11LIBDIR}/%l/%T/%N%S:${X11LIBDIR}/%T/%N%S\"
CPPFLAGS.Error.c=	-DERRORDB=\"${X11LIBDIR}/XtErrorDB\" \
			-DXTERROR_PREFIX=\"\" -DXTWARNING_PREFIX=\"\"
CPPFLAGS.TMkey.c=	-DXKB

LIBDPLIBS=\
	X11	${.CURDIR}/../libX11/dynamic \
	SM	${.CURDIR}/../libSM \
	ICE	${.CURDIR}/../libICE

.if defined(__MINIX)
LIBDPLIBS+=\
	mthread	${NETBSDSRCDIR}/minix/lib/libmthread
.endif # defined(__MINIX)

.include "${NETBSDSRCDIR}/external/mit/xorg/tools/makestrs/Makefile.makestrs"

STRINGS_FILES=	StringDefs.c StringDefs.h Shell.h
STRINGS_TMPLS=	util/StrDefs.ct util/StrDefs.ht util/Shell.ht
STRINGS_LIST=	util/string.list

${STRINGS_FILES}: strings_files.stamp
strings_files.stamp: ${MAKESTRS} ${STRINGS_LIST} ${STRINGS_TMPLS}
	${_MKMSG_CREATE} ${STRINGS_FILES}
	rm -f ${.TARGET} ${STRINGS_FILES}
	rm -f util
	ln -s ${X11SRCDIR.Xt}/util util
	${MAKESTRS} -i . < ${STRINGS_LIST} > StringDefs.c
	touch ${.TARGET}
CLEANFILES+=	${STRINGS_FILES} util strings_files.stamp
DPSRCS+=	${STRINGS_FILES}

MAN=	XtAddActions.3 XtAddCallback.3 XtAddEventHandler.3 \
	XtAddExposureToRegion.3 XtAddGrab.3 XtAddInput.3 XtAllocateGC.3 \
	XtAppAddActionHook.3 XtAppAddActions.3 XtAppAddBlockHook.3 \
	XtAppAddConverter.3 XtAppAddInput.3 XtAppAddSignal.3 \
	XtAppAddTimeOut.3 XtAppAddWorkProc.3 XtAppCreateShell.3 \
	XtAppError.3 XtAppErrorMsg.3 XtAppGetErrorDatabase.3 \
	XtAppGetSelectionTimeout.3 XtAppInitialize.3 XtAppLock.3 \
	XtAppNextEvent.3 XtAppReleaseCacheRefs.3 XtAppSetExitFlag.3 \
	XtAppSetFallbackResources.3 XtAppSetTypeConverter.3 \
	XtBuildEventMask.3 XtCallAcceptFocus.3 XtCallActionProc.3 \
	XtCallCallbacks.3 XtClass.3 XtConfigureWidget.3 XtConvert.3 \
	XtConvertAndStore.3 XtCreateApplicationContext.3 \
	XtCreateApplicationShell.3 XtCreatePopupShell.3 \
	XtCreateSelectionRequest.3 XtCreateWidget.3 XtCreateWindow.3 \
	XtDisplay.3 XtDisplayInitialize.3 XtDisplayStringConversionWarning.3 \
	XtDisplayToApplicationContext.3 XtError.3 XtErrorMsg.3 XtFindFile.3 \
	XtGetActionKeysym.3 XtGetActionList.3 XtGetApplicationNameAndClass.3 \
	XtGetApplicationResources.3 XtGetClassExtension.3 XtGetDisplays.3 \
	XtGetErrorDatabase.3 XtGetGC.3 XtGetKeyboardFocusWidget.3 \
	XtGetKeysymTable.3 XtGetResourceList.3 XtGetSelectionParameters.3 \
	XtGetSelectionRequest.3 XtGetSelectionTimeout.3 XtGetSelectionValue.3 \
	XtGetSelectionValueIncremental.3 XtGetSubresources.3 XtGrabKey.3 \
	XtHooksOfDisplay.3 XtInitialize.3 XtInitializeWidgetClass.3 \
	XtInsertEventTypeHandler.3 XtLastEventProcessed.3 \
	XtMakeGeometryRequest.3 XtMalloc.3 XtManageChildren.3 XtMapWidget.3 \
	XtName.3 XtNameToWidget.3 XtNextEvent.3 XtOffset.3 \
	XtOpenApplication.3 XtOwnSelection.3 XtParent.3 \
	XtParseAcceleratorTable.3 XtParseTranslationTable.3 XtPopdown.3 \
	XtPopup.3 XtProcessLock.3 XtQueryGeometry.3 XtRealizeWidget.3 \
	XtRegisterDrawable.3 XtRegisterGrabAction.3 XtReservePropertyAtom.3 \
	XtResolvePathname.3 XtSessionGetToken.3 XtSetArg.3 \
	XtSetKeyTranslator.3 XtSetKeyboardFocus.3 XtSetLanguageProc.3 \
	XtSetMultiClickTime.3 XtSetSelectionParameters.3 XtSetSensitive.3 \
	XtSetValues.3 XtSetWMColormapWindows.3 XtStringConversionWarning.3 \
	XtToolkitThreadInitialize.3 XtTranslateCoords.3 XtVaCreateArgsList.3

MLINKS= 	XtAddCallback.3 XtAddCallbacks.3 \
		XtAddCallback.3 XtRemoveCallback.3 \
		XtAddCallback.3 XtRemoveCallbacks.3 \
		XtAddCallback.3 XtRemoveAllCallbacks.3

MLINKS+=	XtAddEventHandler.3 XtAddRawEventHandler.3 \
		XtAddEventHandler.3 XtRemoveEventHandler.3 \
		XtAddEventHandler.3 XtRemoveRawEventHandler.3 \
		XtAddEventHandler.3 XtInsertEventHandler.3 \
		XtAddEventHandler.3 XtInsertRawEventHandler.3

MLINKS+=	XtAddGrab.3 XtRemoveGrab.3 \
		XtAppAddConverter.3 XtAddConverter.3 \
		XtAppAddInput.3 XtRemoveInput.3 \
		XtAppAddTimeOut.3 XtRemoveTimeOut.3 \
		XtAppAddWorkProc.3 XtRemoveWorkProc.3 \
		XtAppCreateShell.3 XtVaAppCreateShell.3

MLINKS+=	XtAppError.3 XtAppSetErrorHandler.3 \
		XtAppError.3 XtAppSetWarningHandler.3 \
		XtAppError.3 XtAppWarning.3

MLINKS+=	XtAppErrorMsg.3 XtAppSetErrorMsgHandler.3 \
		XtAppErrorMsg.3 XtAppSetWarningMsgHandler.3 \
		XtAppErrorMsg.3 XtAppWarningMsg.3

MLINKS+=	XtAppGetErrorDatabase.3 XtAppGetErrorDatabaseText.3 \
		XtAppGetSelectionTimeout.3 XtAppSetSelectionTimeout.3

MLINKS+=	XtAppNextEvent.3 XtAppPending.3 \
		XtAppNextEvent.3 XtAppPeekEvent.3 \
		XtAppNextEvent.3 XtAppProcessEvent.3 \
		XtAppNextEvent.3 XtDispatchEvent.3 \
		XtAppNextEvent.3 XtAppMainLoop.3

MLINKS+=	XtNextEvent.3 XtPending.3 \
		XtNextEvent.3 XtPeekEvent.3 \
		XtNextEvent.3 XtProcessEvent.3 \
		XtNextEvent.3 XtMainLoop.3

MLINKS+=	XtCallCallbacks.3 XtCallCallbackList.3 \
		XtCallCallbacks.3 XtHasCallbacks.3

MLINKS+=	XtClass.3 XtSuperclass.3 \
		XtClass.3 XtIsSubclass.3 \
		XtClass.3 XtCheckSubclass.3 \
		XtClass.3 XtIsObject.3 \
		XtClass.3 XtIsRectObj.3 \
		XtClass.3 XtIsWidget.3 \
		XtClass.3 XtIsComposite.3 \
		XtClass.3 XtIsConstraint.3 \
		XtClass.3 XtIsShell.3 \
		XtClass.3 XtIsOverrideShell.3 \
		XtClass.3 XtIsWMShell.3 \
		XtClass.3 XtIsVendorShell.3 \
		XtClass.3 XtIsTransientShell.3 \
		XtClass.3 XtIsTopLevelShell.3 \
		XtClass.3 XtIsApplicationShell.3 \
		XtClass.3 XtIsSessionShell.3

MLINKS+=	XtConfigureWidget.3 XtMoveWidget.3 \
		XtConfigureWidget.3 XtResizeWidget.3

MLINKS+=	XtConvert.3 XtDirectConvert.3 \
		XtConvertAndStore.3 XtCallConverter.3

MLINKS+=	XtCreateApplicationContext.3 XtDestroyApplicationContext.3 \
		XtCreateApplicationContext.3 XtWidgetToApplicationContext.3 \
		XtCreateApplicationContext.3 XtToolkitInitialize.3

MLINKS+=	XtCreatePopupShell.3 XtVaCreatePopupShell.3

MLINKS+=	XtCreateWidget.3 XtVaCreateWidget.3 \
		XtCreateWidget.3 XtCreateManagedWidget.3 \
		XtCreateWidget.3 XtVaCreateManagedWidget.3 \
		XtCreateWidget.3 XtDestroyWidget.3

MLINKS+=	XtDisplay.3 XtDisplayOfObject.3 \
		XtDisplay.3 XtScreen.3 \
		XtDisplay.3 XtScreenOfObject.3 \
		XtDisplay.3 XtWindow.3 \
		XtDisplay.3 XtWindowOfObject.3

MLINKS+=	XtDisplayInitialize.3 XtOpenDisplay.3 \
		XtDisplayInitialize.3 XtCloseDisplay.3 \
		XtDisplayInitialize.3 XtDatabase.3 \
		XtDisplayInitialize.3 XtScreenDatabase.3

MLINKS+=	XtGetGC.3 XtReleaseGC.3 \
		XtGetResourceList.3 XtGetConstraintResourceList.3 \
		XtGetSelectionValue.3 XtGetSelectionValues.3 \
		XtGetSubresources.3 XtVaGetSubresources.3 \
		XtGetApplicationResources.3 XtVaGetApplicationResources.3 \
		XtMakeGeometryRequest.3 XtMakeResizeRequest.3

MLINKS+=	XtMalloc.3 XtCalloc.3 \
		XtMalloc.3 XtRealloc.3 \
		XtMalloc.3 XtFree.3 \
		XtMalloc.3 XtNew.3 \
		XtMalloc.3 XtNewString.3 \
		XtMalloc.3 XtAsprintf.3

MLINKS+=	XtManageChildren.3 XtManageChild.3 \
		XtManageChildren.3 XtUnmanageChildren.3 \
		XtManageChildren.3 XtUnmanageChild.3 \
		XtManageChildren.3 XtIsManaged.3 \
		XtManageChildren.3 XtChangeManagedSet.3

MLINKS+=	XtMapWidget.3 XtSetMappedWhenManaged.3 \
		XtMapWidget.3 XtUnmapWidget.3

MLINKS+=	XtNameToWidget.3 XtWindowToWidget.3

MLINKS+=	XtOffset.3 XtOffsetOf.3 \
		XtOffset.3 XtNumber.3

MLINKS+=	XtOwnSelection.3 XtOwnSelectionIncremental.3 \
		XtOwnSelection.3 XtDisownSelection.3

MLINKS+=	XtParseAcceleratorTable.3 XtInstallAccelerators.3 \
		XtParseAcceleratorTable.3 XtInstallAllAccelerators.3

MLINKS+=	XtParseTranslationTable.3 XtAugmentTranslations.3 \
		XtParseTranslationTable.3 XtOverrideTranslations.3 \
		XtParseTranslationTable.3 XtUninstallTranslations.3

MLINKS+=	XtPopdown.3 XtCallbackPopdown.3 \
		XtPopdown.3 MenuPopdown.3

MLINKS+=	XtPopup.3 XtPopupSpringLoaded.3 \
		XtPopup.3 XtCallbackNone.3 \
		XtPopup.3 XtCallbackNonexclusive.3 \
		XtPopup.3 XtCallbackExclusive.3 \
		XtPopup.3 MenuPopup.3

MLINKS+=	XtRealizeWidget.3 XtIsRealized.3 \
		XtRealizeWidget.3 XtUnrealizeWidget.3

MLINKS+=	XtSetArg.3 XtMergeArgLists.3

MLINKS+=	XtSetKeyTranslator.3 XtTranslateKeycode.3 \
		XtSetKeyTranslator.3 XtRegisterCaseConverter.3 \
		XtSetKeyTranslator.3 XtConvertCase.3

MLINKS+=	XtSetSensitive.3 XtIsSensitive.3

MLINKS+=	XtSetValues.3 XtVaSetValues.3 \
		XtSetValues.3 XtSetSubvalues.3 \
		XtSetValues.3 XtGetValues.3 \
		XtSetValues.3 XtVaGetValues.3 \
		XtSetValues.3 XtGetSubvalues.3 \
		XtSetValues.3 XtVaGetSubvalues.3 \
		XtSetValues.3 XtVaSetSubvalues.3

MLINKS+=	XtGetKeysymTable.3 XtKeysymToKeycodeList.3 \
		XtAppSetTypeConverter.3 XtSetTypeConverter.3 \
		XtAppAddActionHook.3 XtRemoveActionHook.3 \
		XtSetMultiClickTime.3 XtGetMultiClickTime.3

MLINKS+=	XtInsertEventTypeHandler.3 XtRemoveEventTypeHandler.3 \
		XtInsertEventTypeHandler.3 XtRegisterExtensionSelector.3 \
		XtInsertEventTypeHandler.3 XtSetEventDispatcher.3 \
		XtInsertEventTypeHandler.3 XtDispatchEventToWidget.3

MLINKS+=	XtLastEventProcessed.3 XtLastTimestampProcessed.3

MLINKS+=	XtAppAddSignal.3 XtNoticeSignal.3 \
		XtAppAddSignal.3 XtRemoveSignal.3

MLINKS+=	XtAddInput.3 XtAddTimeOut.3 \
		XtAddInput.3 XtAddWorkProc.3

MLINKS+=	XtAppAddBlockHook.3 XtRemoveBlockHook.3 \
		XtAppInitialize.3 XtVaAppInitialize.3 \
		XtSessionGetToken.3 XtSessionReturnToken.3

MLINKS+=	XtErrorMsg.3 XtSetErrorMsgHandler.3 \
		XtErrorMsg.3 XtSetWarningMsgHandler.3 \
		XtErrorMsg.3 XtWarningMsg.3

MLINKS+=	XtError.3 XtSetErrorHandler.3 \
		XtError.3 XtSetWarningHandler.3 \
		XtError.3 XtWarning.3

MLINKS+=	XtGetErrorDatabase.3 XtGetErrorDatabaseText.3 \
		XtGetSelectionValueIncremental.3 XtGetSelectionValuesIncremental.3 \
		XtGetSelectionTimeout.3 XtSetSelectionTimeout.3

MLINKS+=	XtCreateSelectionRequest.3 XtSendSelectionRequest.3 \
		XtCreateSelectionRequest.3 XtCancelSelectionRequest.3

MLINKS+=	XtReservePropertyAtom.3 XtReleasePropertyAtom.3

MLINKS+=	XtGrabKey.3 XtUngrabKey.3 \
		XtGrabKey.3 XtGrabKeyboard.3 \
		XtGrabKey.3 XtUngrabKeyboard.3 \
		XtGrabKey.3 XtGrabButton.3 \
		XtGrabKey.3 XtUngrabButton.3 \
		XtGrabKey.3 XtGrabPointer.3 \
		XtGrabKey.3 XtUngrabPointer.3

MLINKS+=	XtRegisterDrawable.3 XtUnregisterDrawable.3 \
		XtAppSetExitFlag.3 XtAppGetExitFlag.3 \
		XtAppLock.3 XtAppUnlock.3 \
		XtProcessLock.3 XtProcessUnlock.3 \
		XtOpenApplication.3 XtVaOpenApplication.3

PKGDIST=	${LIB}

CWARNFLAGS.clang+=	-Wno-tautological-compare

.include <bsd.x11.mk>
.include <bsd.lib.mk>
