/*
 * Copyright (c) 2018 Kay Gawlik <kaydev@amarunet.de> <kay.gawlik@beuth-hochschule.de> <kay.gawlik@charite.de>
 *
 * This program is free software: you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation, either version 3 of the License, or
 * (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program.  If not, see <http://www.gnu.org/licenses/>.
 *
 */

#ifndef OCTMARKERMAINWINDOW_H
#define OCTMARKERMAINWINDOW_H

#include<functional>

#include<QMainWindow>

#include"octmarkeractions.h"

class QLabel;
class QString;
class QAction;
class QFileDialog;
class QProgressBar;
class QUrl;
class QMenu;
class CVImageWidget;
class OptionColor;

class BScanMarkerWidget;
class DWSloImage;
class CScan;
class ScrollAreaPan;
class DWDebugOutput;

class PaintMarker;



/**
 * @ingroup Windows
 * @brief The main window of OCT-Marker
 */
class OCTMarkerMainWindow : public QMainWindow
{
	Q_OBJECT

	static void messageOutput(QtMsgType type, const QMessageLogContext& context, const QString& msg);

	void setupMenu();
	void setupStatusBar();

	void createMarkerToolbar();

	QDockWidget*          dwSloImage                  = nullptr;
	ScrollAreaPan*        bscanMarkerWidgetScrollArea = nullptr;
	BScanMarkerWidget*    bscanMarkerWidget           = nullptr;
	static DWDebugOutput* dwDebugOutput;

	PaintMarker* pmm = nullptr;

	QProgressBar* loadProgressBar  = nullptr;

	OctMarkerActions generalMarkerActions;
	QList<QAction*> markerActions;
	void generateMarkerActions();

	static void setMarkersFilters(QFileDialog& fd);

	void closeEvent(QCloseEvent* e) override;

	void handleOpenUrl(const QUrl& url, bool singleInput);

	bool catchSaveError(std::function<void ()>& saveObj, std::string& errorStr, const QString& unknownErrorMessage);
	void showErrorDialog(bool isError, const std::string& errorMessage);

protected:
	void dropEvent     (QDropEvent     * event) override;
	void dragEnterEvent(QDragEnterEvent* event) override;
	void dragLeaveEvent(QDragLeaveEvent* event) override;
	void dragMoveEvent (QDragMoveEvent * event) override;

public:
	OCTMarkerMainWindow(bool loadLastFile = true);
	~OCTMarkerMainWindow() override;

	bool loadFile(const QString& filename);
	bool addFile(const QString& filename);
	std::size_t loadFolder(const QString& foldername, int numMaxRecursiv = 10);
	
	static void setMarkersStringList(QStringList& filters);

signals:
	void loadLastFile();

private slots:
	void updateWindowTitle();

	void loadFileStatusSlot(bool loading);
	void loadFileProgress(double frac);

	void triggerSaveMarkersDefaultCatchErrors();

public slots:
	virtual void showLoadImageDialog();
	virtual void showImportFromFolderDialog();

	virtual void showLoadMarkersDialog();
	virtual void showSaveMarkersDialog();

	virtual void saveMatlabBinCode();
	virtual void saveMatlabWriteBinCode();

	virtual void showSaveOctScanDialog();
	virtual void showSaveOctSerieDialog();
	virtual void screenshot();
};


class OptionInt;
/**
 * @ingroup HelperClasses
 * @brief Translator form signal without argument to a signal with a integer
 * @todo Replace the usage with IntValueAction
 */
class SendInt : public QObject
{
Q_OBJECT
	int v;
public:
	SendInt(int value) : v(value) {}
	void connectOptions(OptionInt& option, QAction* action);
public slots:
	void recive(bool b = true) { if(b) send(v); }
signals:
	void send(int value);
};


#endif // OCTMARKERMAINWINDOW_H
