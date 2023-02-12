#ifndef MAINWINDOW_H
#define MAINWINDOW_H

#include <QMainWindow>
#include <QMessageBox>
#include <QBitmap>
#include <QPixmap>
#include <QScrollArea>
#include <QHBoxLayout>
#include <QCheckBox>
#include <QLabel>
#include <QMouseEvent>

#include <string>
#include <vector>
#include <thread>
#include <fstream>
#include <unordered_map>
#include <chrono>

#include <unistd.h>

#include <simpic_server/simpic_server.hpp>
#include <simpic_client/simpic_client.hpp>

#include "config.h"

#define DEFAULT_MEDIA_SPACING 3

QT_BEGIN_NAMESPACE
namespace Ui { class MainWindow; }
QT_END_NAMESPACE


class MainWindow : public QMainWindow
{
    Q_OBJECT

public:
    int hamming;
    QString q_directory;

    std::thread local_thread;
    bool local_started;

    std::unordered_map<int, SimpicClientLib::Image*> references;
    std::unordered_map<int, void*> media_references;

    std::vector<int> selected;
    std::vector<std::string> garbage;

    int img_no;

    int whole;
    int part;
    bool updated_bar;
    bool ever_progress_bar;

    bool no_next;
    bool true_no_next;

    static void error(std::string msg);

    QHBoxLayout *new_horizontal_layout;
    QWidget *scroll_area_contents;

    MainWindow(QWidget *parent = nullptr);
    ~MainWindow();

    SimpicClientLib::SimpicClient *sc;
    std::vector<QLabel*> images;
    uint16_t port;
    std::string address;


    void connect(std::string address, int port);
    void set_disconnected(bool closed);
    void clearMedia();
    void gradual_progress(int part, int whole);
    int get_reasonable_ham();

private slots:
    void on_connectButton_clicked();
    void on_nextButton_clicked();
    void on_serverAddress_returnPressed();
    void on_imageClicked(int id);
    void on_localConnection_clicked();

public slots:
    void on_disconnected(bool closed);
    void on_image_received(QString path, int id);
    void on_toggle_next(bool yeah);
    void on_error_msg(QString error);
    void on_local_server_ready();

signals:
    void signal_image_receive(QString path, int id);
    void signal_toggle_next(bool yeah);
    void signal_error_msg(QString error);
    void signal_local_server_ready();
    void signal_disconnected(bool closed);

private:
    Ui::MainWindow *ui;
    bool connected;
    bool eof;

    bool connection_check();

};

class ClickableLabel : public QLabel
{
    Q_OBJECT

public:
    bool selected;
    std::string path;
    int id;
    ClickableLabel(MainWindow *w);

    void mousePressEvent(QMouseEvent *ev);
signals:
    void qlabel_clicked(int id);
};

#endif // MAINWINDOW_H
