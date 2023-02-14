#include "mainwindow.h"
#include "./ui_mainwindow.h"

ClickableLabel::ClickableLabel(MainWindow *w) : QLabel(w)
{
    selected = false;
}

void ClickableLabel::mousePressEvent(QMouseEvent *ev)
{
    /* Right click means open. */
    if (ev->button() == Qt::RightButton)
    {
        std::system((std::string("xdg-open") + std::string(" ") + path + " &").c_str());
        return;
    }

    emit qlabel_clicked(id);
}

MainWindow::MainWindow(QWidget *parent)
    : QMainWindow(parent)
    , ui(new Ui::MainWindow)
{
    /* A lot of variable initialization. */
    local_started = false;
    whole = 0;
    part = 0;
    updated_bar = false;
    ever_progress_bar = false;
    img_no = 0;
    ui->setupUi(this);
    connected = false;
    eof = true;

    /* Required or else the UI looks extremely ugly. */
    /* ... this is a misnomer, since addStretch() has a default parameter which actually adds no stretching... */
    ui->verticalLayout->addStretch();
    ui->scrollArea->setWidgetResizable(true);


    scroll_area_contents = new QWidget();

    /* a lot of skidded UI code, but it werkz.
    https://stackoverflow.com/questions/35193130/how-to-continously-move-series-of-images-from-left-to-right-on-screen-in-qt
    */
    new_horizontal_layout = new QHBoxLayout(scroll_area_contents);
    new_horizontal_layout->setSpacing(1);
    new_horizontal_layout->addStretch();

    ui->scrollArea->setWidget(scroll_area_contents);

    /* Manual SIGNAL-SLOT connections, because I don't trust Qt Autoconnection */
    QObject::connect(
        this,
        SIGNAL(signal_local_server_ready()),
        this,
        SLOT(on_local_server_ready()),
        Qt::ConnectionType::QueuedConnection
    );

    QObject::connect(
        this,
        SIGNAL(signal_disconnected(bool)),
        this,
        SLOT(on_disconnected(bool)),
        Qt::ConnectionType::QueuedConnection
    );

    QObject::connect(
        this,
        SIGNAL(signal_toggle_next(bool)),
        this,
        SLOT(on_toggle_next(bool)),
        Qt::ConnectionType::QueuedConnection
    );

    QObject::connect(
        this,
        SIGNAL(signal_image_receive(QString, int)),
        this,
        SLOT(on_image_received(QString, int)),
        Qt::ConnectionType::QueuedConnection
    );

    QObject::connect(
        this,
        SIGNAL(signal_error_msg(QString)),
        this,
        SLOT(on_error_msg(QString)),
        Qt::ConnectionType::QueuedConnection
    );

    QObject::connect(
        this,
        SIGNAL(signal_update(int)),
        this,
        SLOT(on_update(int)),
        Qt::ConnectionType::QueuedConnection
    );
}

MainWindow::~MainWindow()
{
    delete ui;
}

void MainWindow::error(std::string msg)
{
    QMessageBox qmsgbox;

    qmsgbox.setText(QString::fromStdString(msg));
    qmsgbox.setStandardButtons(QMessageBox::Ok);
    qmsgbox.setDefaultButton(QMessageBox::Ok);
    qmsgbox.setIcon(QMessageBox::Critical);
    qmsgbox.exec();
}

bool MainWindow::connection_check()
{
    /* Display an error message. */
    if (!connected)
    {
        MainWindow::error("This method cannot be called, because you are currently not connected to a server.");
        return false;
    }

    return true;
}

void MainWindow::on_toggle_next(bool yeah)
{
    ui->nextButton->setEnabled(yeah);
}

void MainWindow::on_update(int images)
{
    ui->mediasLoadedLabel->setText(QString::fromStdString((std::string)"Images: " + std::to_string(images)));
    std::this_thread::sleep_for(std::chrono::milliseconds(250));
}

void MainWindow::connect(std::string address, int port)
{
    try
    {
        sc = new SimpicClientLib::SimpicClient(address, port);
        sc->make_connection();
        sc->set_no_data(false);
    }
    catch (SimpicClientLib::simpic_networking_exception &ex)
    {
        MainWindow::error(std::string("Networking exception: ") + ex.what());
        return;
    }
    catch (std::exception &ex)
    {
        MainWindow::error(std::string("General exception: ") + ex.what());
        return;
    }

    connected = true;
    ui->connectButton->setText("Disconnect");

    std::thread connection_thread([this]() mutable -> void {
        std::string dir = this->q_directory.toStdString();

        this->no_next = false;
        this->true_no_next = false;
        this->ever_progress_bar = false;

        try
        {
            bool in_set = false;

            emit this->signal_toggle_next(false);

            this->sc->request(dir, false, this->hamming, (uint8_t)SimpicClientLib::DataTypes::Image, [this, &in_set](void *data, SimpicClientLib::DataTypes type) -> void {
                if (type == SimpicClientLib::DataTypes::Update)
                {
                    struct SimpicClientLib::UpdateHeader *uh = (struct SimpicClientLib::UpdateHeader*) data;
                    emit this->signal_update(uh->images);
                    return;
                }

                std::string filename = "/tmp/" + SimpicClientLib::random_chars(8);
                /* Beginning of a set. */
                if (!in_set && data == nullptr)
                {
                    emit this->signal_toggle_next(false);

                    this->updated_bar = false;
                    this->selected.clear();
                    this->img_no = 0;

                    in_set = true;
                    return;
                }

                /* End of a set. */
                if (in_set && data == nullptr)
                {
                    emit this->signal_toggle_next(true);

                    in_set = false;
                    return;
                }

                switch (type)
                {
                    case SimpicClientLib::DataTypes::Image:
                    {
                        SimpicClientLib::Image *img = (SimpicClientLib::Image*) data;

                        this->whole = img->no_sets;
                        this->part = img->set_no + 1;

                        std::ofstream output(filename, std::ios::binary);

                        /* Write the incoming file to the disk */
                        char buffer[4096];
                        size_t amnt = 0;
                        while ((amnt = img->readbytes(buffer, sizeof(buffer))) != -1)
                        {
                            output.write(buffer, amnt);
                        }

                        output.flush();
                        output.close();

                        this->references[this->img_no] = img;

                        emit signal_image_receive(QString::fromStdString(filename), this->img_no);
                        this->img_no++;

                        break;
                    }
                }

            });
        }
        catch (SimpicClientLib::simpic_networking_exception &ex)
        {
            emit signal_error_msg(QString::fromStdString("Network error: " + ex.what()));
            emit signal_disconnected(true);
        }
        catch (SimpicClientLib::ErrnoException &ex)
        {
            emit signal_disconnected(false);
            emit signal_error_msg(QString::fromStdString("An errno was raised: " + ex.what()));
        }
        catch (SimpicClientLib::NoResultsException &ex)
        {
            emit signal_error_msg(QString::fromStdString("No results were found... try another directory?"));
            emit signal_disconnected(false);
        }
        catch (SimpicClientLib::InUseException &ex)
        {
            emit signal_error_msg(QString::fromStdString("The directory chosen is already being scanned, cannot continue. "));
            emit signal_disconnected(false);
        }

        this->no_next = true;
    });

    connection_thread.detach();
}

void MainWindow::on_disconnected(bool closed)
{
    clearMedia();
    if (!closed)
    {
        sc->close();
    }

    ui->connectButton->setText("Connect");
    ui->progressBar->setValue(0);

    this->connected = false;
}

void MainWindow::on_connectButton_clicked()
{   
    /* Disconnect button */
    if (this->connected)
    {
        emit signal_disconnected(false);
        return;
    }

    q_directory = ui->directory->text();
    QString str = ui->serverAddress->text();

    if (str.isEmpty())
    {
        error("The server address is empty. Cannot proceed.");
        return;
    }

    if (str == "local")
    {
        hamming = get_reasonable_ham();
        if (hamming == -1)
            return;

        if (!this->local_started)
        {
            local_thread = std::thread([this]() -> void {
                try
                {
                    SimpicServerLib::SimpicServer srv(
                        33422,
                        SimpicServerLib::simpic_folder(SimpicServerLib::home_folder()),
                        SimpicServerLib::simpic_folder(SimpicServerLib::home_folder()) + "recycling_bin/"
                    );

                    this->local_started = true;

                    try
                    {
                        srv.start();
                    }
                    catch (SimpicServerLib::simpic_networking_exception &ex)
                    {
                        emit this->signal_error_msg(QString::fromStdString("Error in local connection: " + ex.what()));
                        this->local_started = false;
                    }
                }
                catch (SimpicServerLib::SimpicMultipleInstanceException &ex)
                {
                    emit this->signal_error_msg(QString::fromStdString(
                        "A Simpic server is already running on this machine, cannot host a local instance."
                    ));
                    return;
                }

            });

            local_thread.detach();

            /* I was hoping I wouldn't have to do this, but it just werkz. */
            /* The model I tried (an event when Simpic server is ready to accept connections) just yielded */
            /* Segmentation Fault over and over again, and I didn't want to deal with that bullshit. */
            /* The SimpicServerLib::SimpicServer class has a member std::function<void()> on_ready */
            /* callback that will fire whenever the server is ready to accept() connections. */
            /* If you want to make it work elegantly, then by all means: do it for me and make a pull request! */
            std::this_thread::sleep_for(std::chrono::seconds(2));
        }

        connect("0.0.0.0", 33422);
        return;
    }

    std::vector<std::string> tokens = SimpicServerLib::split_string(str.toStdString(), ":");

    if (tokens.size() != 2)
    {
        error("The address was not in the format of addr:port.");
        return;
    }

    address = tokens[0];

    try
    {
        port = std::stoi(tokens[1]);
    }
    catch (std::out_of_range &ex)
    {
        error(std::string("Error in parsing the port: ") + ex.what());
        return;
    }

    hamming = get_reasonable_ham();
    if (hamming == -1)
        return;

    connect(address, port);
}

void MainWindow::on_image_received(QString path, int id)
{   
    ClickableLabel *qimg = new ClickableLabel(this);
    qimg->path = path.toStdString();
    qimg->id = id;

    media_references[id] = (void*)qimg;

    SimpicClientLib::Image *img = references[id];
    std::string dimensions = std::to_string(img->width) + "x" + std::to_string(img->height);


    qimg->setToolTip(QString::fromStdString(dimensions));

    QPixmap pixmap(path);

    qimg->setPixmap(pixmap.scaled(320, 320));
    qimg->setMask(pixmap.mask());
    qimg->setScaledContents(false);

    this->new_horizontal_layout->insertWidget(0, qimg);
    qimg->show();
    this->show();

    garbage.push_back(path.toStdString());

    QObject::connect(qimg, SIGNAL(qlabel_clicked(int)), this, SLOT(on_imageClicked(int)));

    if (!ever_progress_bar)
    {
        this->ui->progressBar->setMinimum(0);
        this->ui->progressBar->setMaximum(this->whole);

        ever_progress_bar = true;
    }

    if (!updated_bar)
    {
        updated_bar = true;
        ui->progressBar->setValue(part);
    }

}

void MainWindow::on_imageClicked(int id)
{
    ClickableLabel *label = (ClickableLabel*) media_references[id];

    if (!label->selected)
    {
        selected.push_back(id);
        label->setStyleSheet("border: 1px solid red;");
        label->selected = true;
    }
    else
    {
        selected.erase(std::find(
            selected.begin(),
            selected.end(),
            id
        ));

        label->setStyleSheet("");
        label->selected = false;
    }
}

int MainWindow::get_reasonable_ham()
{
    int result = 0;

    try
    {
        result = std::stoi(ui->hammingDistance->text().toStdString());

        if (!(0 < result && result <= 6))
        {
            MainWindow::error((std::string)"Hamming distance is outside of the reasonable range (1-6)");
            return -1;
        }

        return result;
    }
    catch (std::exception &ex)
    {
        MainWindow::error((std::string)"Error parsing hamming distance: " + ex.what());
        return -1;
    }
}

void MainWindow::clearMedia()
{
    for (std::string &item : garbage)
        std::remove(item.c_str());

    garbage.clear();
    images.clear();

    QLayoutItem *tmp = nullptr;

    /* Delete every item in the QHBoxlayout. */
    while ((tmp = new_horizontal_layout->layout()->takeAt(0)) != nullptr)
    {
        if (tmp->widget() != nullptr)
            delete tmp->widget();

        if (tmp->layout() != nullptr)
            delete tmp->layout();

        if (tmp != nullptr)
            delete tmp;
    }

    if (new_horizontal_layout != nullptr)
        delete new_horizontal_layout;

    new_horizontal_layout = new QHBoxLayout(scroll_area_contents);
    new_horizontal_layout->setSpacing(2);
    new_horizontal_layout->addStretch();

    media_references.clear();
    references.clear();
}

void MainWindow::on_nextButton_clicked()
{
    ui->nextButton->setEnabled(false);

    if (!references.size())
    {
        error("There are no other results found.");
        return;
    }

    if (!connection_check())
        return;

    clearMedia();

    if (selected.size() != 0)
    {
        sc->remove(selected);
        selected.clear();
    }
    else
        sc->keep();
}


void MainWindow::on_error_msg(QString error)
{
    MainWindow::error(error.toStdString());
}

void MainWindow::on_serverAddress_returnPressed()
{
    on_connectButton_clicked();
}


void MainWindow::on_local_server_ready()
{
    /*
    q_directory = ui->directory->text();
    connect("0.0.0.0", 33422);
    */
}
