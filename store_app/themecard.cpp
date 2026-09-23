#include "themecard.h"

#include <QVBoxLayout>
#include <QHBoxLayout>
#include <QPixmap>
#include <QBuffer>
#include <QMovie>
#include <QImageReader>
#include <QDesktopServices>
#include <QMouseEvent>
#include <QStyle>
#include <QLocale>
#include <QSizePolicy>
#include <QResizeEvent>
#include <QShowEvent>
#include <QFontMetrics>
#include <QTextLayout>
#include <QTextOption>
#include <QTimer>

namespace {

// Height (px) reserved for the description label: exactly three lines of the
// label's font.
int descriptionHeight(const QFont &font)
{
    return QFontMetrics(font).lineSpacing() * 3;
}

// Wraps `text` to `width` and returns only the first `maxLines` lines, eliding
// the last one with "…" when it would be clipped. Builds the result directly
// (no intermediate list) so it is safe on COW/shared Qt containers.
QString elideText(const QString &text, const QFont &font, int width, int maxLines)
{
    if (width <= 0 || maxLines <= 0)
        return {};

    QTextLayout layout(text, font);
    QTextOption option;
    option.setWrapMode(QTextOption::WrapAtWordBoundaryOrAnywhere);
    layout.setTextOption(option);

    QString result;
    layout.beginLayout();
    int lineIndex = 0;
    while (true) {
        QTextLine line = layout.createLine();
        if (!line.isValid())
            return result; // all text fits within maxLines; no elision needed

        line.setLineWidth(width);
        const QString lineText = text.mid(line.textStart(), line.textLength());

        if (lineIndex == maxLines - 1) {
            // If the layout can still produce another line, the text is being
            // truncated and the last visible line must end with an ellipsis.
            // Elide with "…" appended explicitly so the marker is always
            // emitted, even if that line happens to fit `width` on its own.
            if (layout.createLine().isValid())
                result += QFontMetrics(font).elidedText(
                    lineText + QStringLiteral("\u2026"), Qt::ElideRight, width);
            else
                result += lineText;
            return result;
        }

        result += lineText;
        result += QLatin1Char('\n');
        ++lineIndex;
    }
}

} // namespace

ThemeCard::ThemeCard(const ThemeData &theme, QWidget *parent)
    : QFrame(parent)
    , m_theme(theme)
{
    setObjectName("themeCard");
    setFrameShape(QFrame::NoFrame);
    setCursor(Qt::PointingHandCursor);
    setMinimumWidth(200);
    setFixedHeight(380);
    setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Fixed);

    auto *layout = new QVBoxLayout(this);
    layout->setContentsMargins(0, 0, 0, 0);
    layout->setSpacing(0);

    m_thumbnailLabel = new QLabel(this);
    m_thumbnailLabel->setObjectName("themeThumbnail");
    m_thumbnailLabel->setFixedHeight(180);
    m_thumbnailLabel->setAlignment(Qt::AlignCenter);
    m_thumbnailLabel->setStyleSheet(
        "background: #2a2a2e; color: #8b8b90; border-radius: 10px 10px 0 0;");
    m_thumbnailLabel->setText(QStringLiteral("Loading..."));
    layout->addWidget(m_thumbnailLabel);

    auto *body = new QWidget(this);
    body->setObjectName("themeBody");
    auto *bodyLayout = new QVBoxLayout(body);
    bodyLayout->setContentsMargins(12, 10, 12, 10);
    bodyLayout->setSpacing(6);

    auto *nameLabel = new QLabel(m_theme.name, body);
    nameLabel->setObjectName("themeName");
    nameLabel->setWordWrap(true);
    nameLabel->setStyleSheet(
        "font-size: 14px; font-weight: 700; color: #f2f2f4; background: transparent;");
    bodyLayout->addWidget(nameLabel);

    auto *authorLabel = new QLabel(QStringLiteral("by %1")
        .arg(m_theme.author), body);
    authorLabel->setObjectName("themeAuthor");
    authorLabel->setStyleSheet(
        "font-size: 11px; color: #9a9aa2; background: transparent;");
    bodyLayout->addWidget(authorLabel);

    m_descFullText = m_theme.description.isEmpty()
        ? m_theme.tags.join(QStringLiteral(" \u00B7 "))
        : m_theme.description;
    m_descLabel = new QLabel(m_descFullText, body);
    m_descLabel->setObjectName("themeDesc");
    m_descLabel->setWordWrap(true);
    m_descLabel->setAlignment(Qt::AlignTop | Qt::AlignLeft);
    m_descLabel->setFixedHeight(descriptionHeight(m_descLabel->font()));
    m_descLabel->setStyleSheet(
        "font-size: 11px; color: #c0c0c6; background: transparent;");
    bodyLayout->addWidget(m_descLabel);

    auto *stats = new QHBoxLayout();
    stats->setSpacing(12);

    auto *downloadsLabel = new QLabel(QStringLiteral("\u2B07 %1 downloads")
        .arg(QLocale().toString(m_theme.downloads)), body);
    downloadsLabel->setStyleSheet("font-size: 11px; color: #7aa2f7; background: transparent;");
    stats->addWidget(downloadsLabel);
    stats->addStretch();

    auto *likeLabel = new QLabel(QStringLiteral("\u2665 %1")
        .arg(QLocale().toString(m_theme.likes)), body);
    likeLabel->setObjectName("likeLabel");
    likeLabel->setStyleSheet("font-size: 11px; color: #e05a7a; background: transparent;");
    m_likeLabel = likeLabel;
    stats->addWidget(likeLabel);
    bodyLayout->addLayout(stats);

    auto *btnRow = new QHBoxLayout();
    btnRow->setSpacing(8);

    auto *likeBtn = new QPushButton(QStringLiteral("\u2665"), body);
    likeBtn->setObjectName("likeBtn");
    likeBtn->setCursor(Qt::ArrowCursor);
    likeBtn->setToolTip(QStringLiteral("Like this theme"));
    likeBtn->setMinimumHeight(30);
    likeBtn->setFixedWidth(34);
    likeBtn->setStyleSheet(
        "QPushButton { background: transparent; color: #e05a7a; border: 1px solid #e05a7a;"
        " border-radius: 6px; font-size: 15px; }"
        "QPushButton:hover { background: #3a2028; }"
        "QPushButton:pressed { background: #4a2530; }");
    m_likeBtn = likeBtn;
    btnRow->addWidget(likeBtn);

    auto *installBtn = new QPushButton(QStringLiteral("Install"), body);
    installBtn->setObjectName("installBtn");
    installBtn->setCursor(Qt::ArrowCursor);
    installBtn->setMinimumHeight(30);
    installBtn->setStyleSheet(
        "QPushButton { background: #3d7eff; color: white; border: none; border-radius: 6px;"
        " font-weight: 600; padding: 0 12px; }"
        "QPushButton:hover { background: #4d8eff; }"
        "QPushButton:pressed { background: #2f6ce0; }"
        "QPushButton:disabled { background: #333349; color: #8b93a5; }");
    m_installBtn = installBtn;
    btnRow->addWidget(installBtn);

    auto *removeBtn = new QPushButton(QStringLiteral("Remove"), body);
    removeBtn->setObjectName("removeBtn");
    removeBtn->setCursor(Qt::ArrowCursor);
    removeBtn->setMinimumHeight(30);
    removeBtn->setVisible(false);
    removeBtn->setStyleSheet(
        "QPushButton { background: transparent; color: #f08080; border: 1px solid #b34a4a;"
        " border-radius: 6px; font-weight: 600; padding: 0 10px; }"
        "QPushButton:hover { background: #3a2727; }"
        "QPushButton:pressed { background: #4a2f2f; }");
    m_removeBtn = removeBtn;
    btnRow->addWidget(removeBtn);

    if (m_theme.donateUrl.isValid() && !m_theme.donateUrl.isEmpty()) {
        QString supportText = m_theme.donationLabel.isEmpty()
            ? QStringLiteral("\u2764 Support") : m_theme.donationLabel;
        auto *donateBtn = new QPushButton(supportText, body);
        donateBtn->setObjectName("donateBtn");
        donateBtn->setCursor(Qt::ArrowCursor);
        donateBtn->setMinimumHeight(30);
        donateBtn->setStyleSheet(
            "QPushButton { background: transparent; color: #e05a7a; border: 1px solid #e05a7a;"
            " border-radius: 6px; font-weight: 600; padding: 0 10px; }"
            "QPushButton:hover { background: #3a2028; }");
        btnRow->addWidget(donateBtn);

        connect(donateBtn, &QPushButton::clicked, this, [this]() {
            emit donateRequested(m_theme.donateUrl);
        });
    }

    btnRow->addStretch();
    bodyLayout->addLayout(btnRow);
    layout->addWidget(body);

    connect(installBtn, &QPushButton::clicked, this, [this]() {
        emit installRequested(m_themeId, m_theme);
    });

    connect(removeBtn, &QPushButton::clicked, this, [this]() {
        emit removeRequested(m_themeId, m_theme);
    });

    connect(likeBtn, &QPushButton::clicked, this, [this]() {
        emit likeRequested(m_themeId, m_theme);
    });
}

void ThemeCard::setLikes(qint64 likes)
{
    m_theme.likes = likes;
    if (m_likeLabel)
        m_likeLabel->setText(QStringLiteral("\u2665 %1").arg(QLocale().toString(likes)));
}

void ThemeCard::setInstalled(bool installed)
{
    if (m_installed == installed)
        return;

    m_installed = installed;
    m_installBtn->setVisible(!installed);
    m_removeBtn->setVisible(installed);
}

void ThemeCard::resizeEvent(QResizeEvent *event)
{
    QFrame::resizeEvent(event);
    updateDescriptionElision();
}

void ThemeCard::showEvent(QShowEvent *event)
{
    QFrame::showEvent(event);
    // The first resizeEvent can fire before the layout assigns the label a
    // real width; defer so the description elides against its final width.
    QTimer::singleShot(0, this, [this]() {
        updateDescriptionElision();
    });
}

void ThemeCard::updateDescriptionElision()
{
    if (!m_descLabel)
        return;

    const int width = m_descLabel->width();
    if (width <= 0)
        return; // not laid out yet; keep the text as-is

    m_descLabel->setText(elideText(m_descFullText, m_descLabel->font(), width, 3));
}

void ThemeCard::setThumbnailFailed()
{
    clearMovie();
    m_thumbnailLabel->setPixmap(QPixmap());
    m_thumbnailLabel->setText(QStringLiteral("No preview"));
    m_thumbnailData.clear();
}

void ThemeCard::clearMovie()
{
    if (m_movie) {
        m_movie->stop();
        m_thumbnailLabel->setMovie(nullptr);
        delete m_movie;
        m_movie = nullptr;
    }
    if (m_movieBuffer) {
        delete m_movieBuffer;
        m_movieBuffer = nullptr;
    }
}

static bool isGif(const QByteArray &data)
{
    return data.startsWith("GIF87a") || data.startsWith("GIF89a");
}

void ThemeCard::setThumbnail(const QByteArray &imageData)
{
    if (imageData.isEmpty()) {
        clearMovie();
        m_thumbnailLabel->setText(QStringLiteral("No preview"));
        return;
    }

    if (isGif(imageData)) {
        m_thumbnailLabel->setText(QString());
        m_thumbnailLabel->setPixmap(QPixmap());

        clearMovie();

        auto *buffer = new QBuffer(this);
        buffer->setData(imageData);
        buffer->open(QIODevice::ReadOnly);

        QSize frame;
        {
            QImageReader probe(buffer);
            frame = probe.size();
        }
        buffer->seek(0);
        if (!frame.isValid() || frame.isEmpty())
            frame = QSize(m_thumbnailLabel->width(), m_thumbnailLabel->height());
        // Expand-and-crop so the animation always covers the whole thumbnail
        // area, matching the static-image path below.
        frame.scale(m_thumbnailLabel->width(), m_thumbnailLabel->height(), Qt::KeepAspectRatioByExpanding);

        auto *movie = new QMovie(buffer, QByteArray(), this);
        movie->setScaledSize(frame);
        m_thumbnailLabel->setMovie(movie);
        movie->start();

        m_movieBuffer = buffer;
        m_movie = movie;
        m_thumbnailData = imageData;
        return;
    }

    clearMovie();

    QPixmap pix;
    if (!pix.loadFromData(imageData)) {
        m_thumbnailLabel->setText(QStringLiteral("No preview"));
        return;
    }

    m_thumbnailLabel->setPixmap(pix.scaled(
        m_thumbnailLabel->size(), Qt::KeepAspectRatioByExpanding, Qt::SmoothTransformation));
    m_thumbnailData = imageData;
}