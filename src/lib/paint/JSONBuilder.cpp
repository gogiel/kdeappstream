#include "JSONBuilder.h"

#include "paint/PaintDevice.h"
#include "paint/PaintEngine.h"
#include "websocket/QWsSocket.h"

#include <QString>
#include <QWidget>
#include <QTimer>
#include <QDebug>
#include <QByteArray>
#include <QBuffer>
#include <QLocalSocket>
#include <QCoreApplication>

using namespace KAppStream;

JSONBuilder * JSONBuilder::m_instance = 0;

JSONBuilder::JSONBuilder(QObject * parent) :
    QObject(parent),
    _sem(1)
{
    buffer.reserve(10000);
}

JSONBuilder * JSONBuilder::instance(QObject * parent)
{
    if (!m_instance)
        m_instance = new JSONBuilder(parent);
    return m_instance;
}

void JSONBuilder::beginRender(QWidget * widget)
{
    if (!widget)
        return;

    QSize s = widget->size();
    QPoint p = widget->pos();
    if (!widget->parentWidget())
        p = QPoint(0,0);
    else if (widget->windowType() & Qt::Window)
        p = widget->parentWidget()->mapFromGlobal(p);

    context << widget;
    buffer.append("{\"command\":\"draw\"")
          .append(",\"widget\":{")
          .append("\"id\":").append(QString::number((long long)widget).toAscii())
          .append(",\"name\":\"").append(widget->metaObject()->className()).append("\"")
          .append(",\"x\":").append(QString::number( p.x() ).toAscii())
          .append(",\"y\":").append(QString::number( p.y() ).toAscii())
          .append(",\"w\":").append(QString::number( s.width() ).toAscii())
          .append(",\"h\":").append(QString::number( s.height() ).toAscii())
          .append("},\"render\":[");
}

void JSONBuilder::endRender()
{
    if (!context.length())
        return;
    context.removeFirst();
    if (buffer[buffer.length()-1] == ',')
        buffer.remove(buffer.length()-1, 1);
    buffer.append("]},");
}

void JSONBuilder::addChild(QWidget * child, QWidget * parent)
{
    _sem.acquire();
    buffer.append("{\"command\":\"addChild\"")
          .append(",\"id\":").append(QString::number((long long)parent).toAscii())
          .append(",\"child\":").append(QString::number((long long)child).toAscii())
          .append("},");
    _sem.release();
}

void JSONBuilder::removeChild(QWidget * child, QWidget * parent)
{
    _sem.acquire();
    buffer.append("{\"command\":\"removeChild\"")
          .append(",\"id\":").append(QString::number((long long)parent).toAscii())
          .append(",\"child\":").append(QString::number((long long)child).toAscii())
          .append("},");
    _sem.release();
}

void JSONBuilder::finish()
{
    emit readyRead();
}

void JSONBuilder::flush(QWsSocket * device)
{
    _sem.acquire();
    if (device)
    {
        if (buffer.length())
        {
            if (buffer[buffer.length()-1] == ',')
                buffer.remove(buffer.length()-1, 1);
            device->write(QString('[' + buffer + ']'));
            device->waitForBytesWritten(1000);
        }
    }
    buffer.clear();
    _sem.release();
}

void JSONBuilder::ellipse(const QRect & r)
{
    _sem.acquire();
    this->saveStatePriv();
    buffer.append("{\"t\":\"ellipse\"")
          .append(",\"x\":").append(QString::number(r.x()))
          .append(",\"y\":").append(QString::number(r.y()))
          .append(",\"w\":").append(QString::number(r.width()))
          .append(",\"h\":").append(QString::number(r.height()))
          .append("},");
    _sem.release();
}

void JSONBuilder::ellipse(const QRectF & r)
{
    _sem.acquire();
    this->saveStatePriv();
    buffer.append("{\"t\":\"ellipse\"")
          .append(",\"x\":").append(QString::number(r.x()))
          .append(",\"y\":").append(QString::number(r.y()))
          .append(",\"w\":").append(QString::number(r.width()))
          .append(",\"h\":").append(QString::number(r.height()))
          .append("},");
    _sem.release();
}

void JSONBuilder::image(const QRectF & r, const QImage & image, const QRectF & sr, Qt::ImageConversionFlags flags)
{
    _sem.acquire();
    this->saveStatePriv();
    QByteArray byteArray;
    QBuffer buf(&byteArray);
    image.copy(sr.toRect()).scaled(r.size().toSize()).save(&buf, "PNG");
    buffer.append("{\"t\":\"image\"")
          .append(",\"data\":\"data:image/png;base64,")
          .append(byteArray.toBase64())
          .append("\",\"x\":").append(QString::number(r.left()).toAscii())
          .append(",\"y\":").append(QString::number(r.top()).toAscii())
          .append("},");
    _sem.release();
}

void JSONBuilder::image(const QImage & i)
{
    _sem.acquire();
    this->saveStatePriv();
    QByteArray byteArray;
    QBuffer buf(&byteArray);
    i.save(&buf, "PNG");
    buffer.append("{\"t\":\"image\"")
          .append(",\"data\":\"data:image/png;base64,")
          .append(byteArray.toBase64())
          .append("\"},");
    _sem.release();
}

void JSONBuilder::line(const QLine & l)
{
    _sem.acquire();
    this->saveStatePriv();
    buffer.append("{\"t\":\"line\"")
          .append(",\"xs\":").append(QString::number(l.x1()))
          .append(",\"ys\":").append(QString::number(l.y1()))
          .append(",\"xe\":").append(QString::number(l.x2()))
          .append(",\"ye\":").append(QString::number(l.y2()))
          .append("},");
    _sem.release();
}

void JSONBuilder::line(const QLineF & l)
{
    _sem.acquire();
    this->saveStatePriv();
    buffer.append("{\"t\":\"line\"")
          .append(",\"xs\":").append(QString::number(l.x1()))
          .append(",\"ys\":").append(QString::number(l.y1()))
          .append(",\"xe\":").append(QString::number(l.x2()))
          .append(",\"ye\":").append(QString::number(l.y2()))
          .append("},");
    _sem.release();
}

void JSONBuilder::path(const QPainterPath & path)
{
    _sem.acquire();
    buffer.append("{\"t\":\"path\"")
            .append(",\"data\":[");

    for (int i = 0; i < path.elementCount(); ++i)
    {
        QPainterPath::Element e = path.elementAt(i);
        buffer.append("{\"t\":").append(QString::number(e.type).toAscii());
        buffer.append(",\"p\":[[")
                .append(QString::number(e.x).toAscii())
                .append(",")
                .append(QString::number(e.y).toAscii())
              .append("],");

        if (e.type == QPainterPath::CurveToElement)
        {
            for (e = path.elementAt(++i); i < path.elementCount(); ++i)
            {
                buffer.append("[")
                        .append(QString::number(e.x).toAscii())
                        .append(",")
                        .append(QString::number(e.y).toAscii())
                        .append("],");
                e = path.elementAt(i);
                if (e.type != QPainterPath::CurveToDataElement)
                {
                    --i;
                    break;
                }
            }
        }

        if (buffer[buffer.length()-1] == ',')
            buffer.remove(buffer.length()-1, 1);

        buffer.append("],\"fill\":").append(QString::number(path.fillRule()).toAscii());
        buffer.append("},");
    }

    if (buffer[buffer.length()-1] == ',')
        buffer.remove(buffer.length()-1, 1);

    buffer.append("]},");
    _sem.release();
}

void JSONBuilder::pixmap(const QRectF & r, const QPixmap & pm, const QRectF & sr)
{
    _sem.acquire();
    this->saveStatePriv();
    QByteArray byteArray;
    QBuffer buf(&byteArray);
    QImage im = pm.copy(sr.toRect()).toImage();
    im.scaled(r.size().toSize()).save(&buf, "PNG");
    buffer.append("{\"t\":\"image\"")
          .append(",\"data\":\"data:image/png;base64,")
          .append(byteArray.toBase64())
          .append("\",\"x\":").append(QString::number(r.left()).toAscii())
          .append(",\"y\":").append(QString::number(r.top()).toAscii())
          .append("},");
    _sem.release();
}

void JSONBuilder::pixmap(const QPixmap & pm)
{
    _sem.acquire();
    this->saveStatePriv();
    QByteArray byteArray;
    QBuffer buf(&byteArray);
    pm.save(&buf, "PNG");
    buffer.append("{\"t\":\"image\"")
          .append(",\"data\":\"data:image/png;base64,")
          .append(byteArray.toBase64())
          .append("\"},");
    _sem.release();
}

void JSONBuilder::points(const QPointF * points, int pointCount)
{
    _sem.acquire();
    buffer.append("{\"t\":\"points\"")
            .append(",\"data\":[");
    while (pointCount--)
    {
        buffer.append("[").append(QString::number(points->x()).toAscii()).append(",").append(QString::number(points->y()).toAscii()).append("],");
        ++points;
    }
    if (buffer[buffer.length()-1] == ',')
        buffer.remove(buffer.length()-1, 1);
    buffer.append("]},");
    _sem.release();
}

void JSONBuilder::points(const QPoint * points, int pointCount)
{
    _sem.acquire();
    buffer.append("{\"t\":\"points\"")
            .append(",\"data\":[");
    while (pointCount--)
    {
        buffer.append("[").append(QString::number(points->x()).toAscii()).append(",").append(QString::number(points->y()).toAscii()).append("],");
        ++points;
    }
    if (buffer[buffer.length()-1] == ',')
        buffer.remove(buffer.length()-1, 1);
    buffer.append("]},");
    _sem.release();
}

void JSONBuilder::polygon(const QPointF * points, int pointCount, QPaintEngine::PolygonDrawMode mode)
{
    _sem.acquire();
    buffer.append("{\"t\":\"polygon\"")
            .append(",\"mode\":").append(QString::number(mode))
            .append(",\"data\":[");
    while (pointCount--)
    {
        buffer.append("[").append(QString::number(points->x()).toAscii()).append(",").append(QString::number(points->y()).toAscii()).append("],");
        ++points;
    }
    if (buffer[buffer.length()-1] == ',')
        buffer.remove(buffer.length()-1, 1);
    buffer.append("]},");
    _sem.release();
}

void JSONBuilder::polygon(const QPoint * points, int pointCount, QPaintEngine::PolygonDrawMode mode)
{
    _sem.acquire();
    buffer.append("{\"t\":\"polygon\"")
            .append(",\"mode\":").append(QString::number(mode))
            .append(",\"data\":[");
    while (pointCount--)
    {
        buffer.append("[").append(QString::number(points->x()).toAscii()).append(",").append(QString::number(points->y()).toAscii()).append("],");
        ++points;
    }
    if (buffer[buffer.length()-1] == ',')
        buffer.remove(buffer.length()-1, 1);
    buffer.append("]},");
    _sem.release();
}

void JSONBuilder::rect(const QRect & r)
{
    _sem.acquire();
    this->saveStatePriv();
    buffer.append("{\"t\":\"rect\"")
          .append(",\"x\":").append(QString::number(r.x()))
          .append(",\"y\":").append(QString::number(r.y()))
          .append(",\"w\":").append(QString::number(r.width()))
          .append(",\"h\":").append(QString::number(r.height()))
          .append("},");
    _sem.release();
}

void JSONBuilder::rect(const QRectF & r)
{
    _sem.acquire();
    this->saveStatePriv();
    buffer.append("{\"t\":\"rect\"")
          .append(",\"x\":").append(QString::number(r.x()))
          .append(",\"y\":").append(QString::number(r.y()))
          .append(",\"w\":").append(QString::number(r.width()))
          .append(",\"h\":").append(QString::number(r.height()))
          .append("},");
    _sem.release();
}

void JSONBuilder::text(const QPointF & p, const QTextItem & textItem)
{
    _sem.acquire();
    buffer.append("{\"t\":\"text\"")
            .append(",\"data\":{")
            .append("\"text\":\"").append(textItem.text().toAscii()).append("\"")
            .append(",\"ascent\":").append(QString::number(textItem.ascent()).toAscii())
            .append(",\"descent\":").append(QString::number(textItem.descent()).toAscii())
            .append(",\"x\":").append(QString::number(p.x()).toAscii())
            .append(",\"y\":").append(QString::number(p.y()).toAscii())
            .append(",");
    font(textItem.font());
    buffer.append("}},");
    _sem.release();
}

void JSONBuilder::tiledPixmap(const QRectF & rect, const QPixmap & pm, const QPointF & p)
{
    _sem.acquire();
    QByteArray byteArray;
    QBuffer buf(&byteArray);
    pm.save(&buf, "PNG");
    this->saveStatePriv();
    buffer.append("{\"t\":\"tiledPixmap\"")
            .append(",\"rect\":{")
            .append("\"x\":").append(QString::number(rect.x()).toAscii())
            .append(",\"y\":").append(QString::number(rect.y()).toAscii())
            .append(",\"w\":").append(QString::number(rect.width()).toAscii())
            .append(",\"h\":").append(QString::number(rect.height()).toAscii())
            .append("}")
            .append(",\"x\":").append(QString::number(p.x()).toAscii())
            .append(",\"y\":").append(QString::number(p.y()).toAscii())
            .append(",\"pixmap\":\"data:image/png;base64,").append(byteArray.toBase64())
          .append("\"},");
    _sem.release();
}

void JSONBuilder::resize(QWidget * w, const QSize & oldSize, const QSize & newSize)
{
    //if (w->windowType() & Qt::Window)
    {
        buffer.append("{\"command\":\"resize\"")
              .append(",\"id\":").append(QString::number((long long)w).toAscii())
              .append(",\"old\":")
                .append("{\"w\":").append(QString::number(oldSize.width()).toAscii())
                .append(",\"h\":").append(QString::number(oldSize.height()).toAscii())
              .append("},\"new\":")
                .append("{\"w\":").append(QString::number(newSize.width()).toAscii())
                .append(",\"h\":").append(QString::number(newSize.height()).toAscii())
              .append("}},");
    }
}

void JSONBuilder::state(const QPaintEngineState & s)
{
    _sem.acquire();
    QPaintEngine::DirtyFlags f = s.state();
    if (f & QPaintEngine::DirtyBackground)
        cur_state.backgroundBrush = s.backgroundBrush();
    if (f & QPaintEngine::DirtyBackgroundMode)
        cur_state.backgroundMode = s.backgroundMode();
    if (f & QPaintEngine::DirtyBrush)
        cur_state.brush = s.brush();
    if (f & QPaintEngine::DirtyBrushOrigin)
        cur_state.brushOrigin = s.brushOrigin();
    if (f & QPaintEngine::DirtyClipPath || f & QPaintEngine::DirtyClipRegion)
        cur_state.clipOperation = s.clipOperation();
    if (f & QPaintEngine::DirtyClipPath)
        cur_state.clipPath = s.clipPath();
    if (f & QPaintEngine::DirtyClipRegion)
        cur_state.clipRegion = s.clipRegion();
    if (f & QPaintEngine::DirtyCompositionMode)
        cur_state.compositionMode = s.compositionMode();
    if (f & QPaintEngine::DirtyFont)
        cur_state.font = s.font();
    if (f & QPaintEngine::DirtyClipEnabled)
        cur_state.isClipEnabled = s.isClipEnabled();
    if (f & QPaintEngine::DirtyOpacity)
        cur_state.opacity = s.opacity();
    if (f & QPaintEngine::DirtyPen)
        cur_state.pen = s.pen();
    if (f & QPaintEngine::DirtyHints)
        cur_state.renderHints = s.renderHints();
    if (f & QPaintEngine::DirtyTransform)
        cur_state.transform = s.transform();
    cur_state.state |= f & ~(QPaintEngine::DirtyHints | QPaintEngine::DirtyClipPath | QPaintEngine::DirtyClipEnabled | QPaintEngine::DirtyClipRegion);
    _sem.release();
}

void JSONBuilder::saveState()
{
    _sem.acquire();
    saveStatePriv();
    _sem.release();
}

void JSONBuilder::saveStatePriv()
{
    QPaintEngine::DirtyFlags f = cur_state.state;
    if (f & QPaintEngine::AllDirty)
    {
        buffer.append("{\"t\":\"state\"")
              .append(",\"data\":{");

        if (f & QPaintEngine::DirtyPen)
        {
            pen(cur_state.pen);
            buffer.append(",");
        }

        if (f & QPaintEngine::DirtyBrush)
        {
            brush(cur_state.brush);
            buffer.append(",");
        }

        if (f & QPaintEngine::DirtyFont)
        {
            font(cur_state.font);
            buffer.append(",");
        }

        if (f & QPaintEngine::DirtyBrushOrigin)
        {
            buffer.append("\"brushorigin\":{\"x\":").append(QString::number(cur_state.brushOrigin.x()))
                  .append(",\"y\":").append(QString::number(cur_state.brushOrigin.y()))
                  .append("},");
        }

        if (f & QPaintEngine::DirtyTransform)
        {
            transform(cur_state.transform);
            buffer.append(",");
        }

        if (f & QPaintEngine::DirtyCompositionMode)
        {
            buffer.append("\"composition\":");
            switch (cur_state.compositionMode)
            {
                case QPainter::CompositionMode_SourceAtop:
                    buffer.append("\"source-atop\":");
                    break;
                case QPainter::CompositionMode_SourceIn:
                    buffer.append("\"source-in\":");
                    break;
                case QPainter::CompositionMode_SourceOut:
                    buffer.append("\"source-out\":");
                    break;
                case QPainter::CompositionMode_SourceOver:
                    buffer.append("\"source-over\":");
                    break;
                case QPainter::CompositionMode_DestinationAtop:
                    buffer.append("\"destination-atop\":");
                    break;
                case QPainter::CompositionMode_DestinationIn:
                    buffer.append("\"destination-in\":");
                    break;
                case QPainter::CompositionMode_DestinationOut:
                    buffer.append("\"destination-out\":");
                    break;
                case QPainter::CompositionMode_DestinationOver:
                    buffer.append("\"destination-over\":");
                    break;
                case QPainter::CompositionMode_Lighten:
                    buffer.append("\"lighter\":");
                    break;
                case QPainter::CompositionMode_Darken:
                    buffer.append("\"darker\":");
                    break;
                case QPainter::CompositionMode_Xor:
                    buffer.append("\"xor\":");
                    break;
                case QPainter::CompositionMode_Source:
                    buffer.append("\"copy\":");
                    break;
                default:
                    buffer.append("\"source-over\":");
                    break;
            }
            buffer.append(",");
        }

        if (f & (QPaintEngine::DirtyBackground | QPaintEngine::DirtyBackgroundMode))
        {
            bbrush(cur_state.backgroundBrush);
            buffer.append(",");
        }

        if (f & QPaintEngine::DirtyOpacity)
        {
            opacity(cur_state.opacity);
            buffer.append(",");
        }

        if (buffer[buffer.length()-1] == ',')
            buffer.remove(buffer.length()-1, 1);
        buffer.append("}},");

        cur_state.state = 0;
    }
}

////////////// HELPER FUNCTIONS BELOW - NOT COLON SEPARATED /////////////////

void JSONBuilder::color(const QColor & c)
{
    buffer.append("\"color\":\"")
          .append(c.name())
          .append("\"");
}

void JSONBuilder::font(const QFont & f)
{
    buffer.append("\"font\":\"");

    switch (f.style())
    {
        case QFont::StyleItalic:
            buffer.append("italic ");
            break;
        case QFont::StyleOblique:
            buffer.append("\"oblique ");
            break;
        default:
            buffer.append("normal ");
            break;
    }

    switch (f.capitalization())
    {
        case QFont::SmallCaps:
            buffer.append("\"small-caps ");
            break;
        default:
            buffer.append("normal ");
            break;
    }

    switch (f.weight())
    {
        case QFont::Light:
            buffer.append("\"lighter ");
            break;
        case QFont::DemiBold:
            buffer.append("\"600 ");
            break;
        case QFont::Bold:
            buffer.append("\"700 ");
            break;
        case QFont::Black:
            buffer.append("\"900 ");
            break;
        default:
            buffer.append("400 ");
            break;
    }

    if (f.pixelSize() != -1)
        buffer.append(QString::number(f.pixelSize()).toAscii()).append("px ");
    else if (f.pointSize() != -1)
        buffer.append(QString::number(f.pointSize()).toAscii()).append("pt ");
    else if (f.pointSizeF() != -1)
        buffer.append(QString::number(f.pointSizeF()).toAscii()).append("pt ");
    else
        buffer.append("medium ");

    buffer.append(f.key().toAscii()).append(",").append(f.family().toAscii()).append(",").append(f.defaultFamily().toAscii());

    buffer.append("\"");
}

void JSONBuilder::opacity(qreal op)
{
    buffer.append("\"opacity\":").append(QString::number(op).toAscii());
}

void JSONBuilder::pen(const QPen & p)
{
    buffer.append("\"pen\":{");

    // If brush defined (instead color)
    QBrush b = p.brush();
    if (b.style() != Qt::NoBrush && b.style() != Qt::SolidPattern)
        brush(p.brush());
    else
        color(p.color());

    if (p.style() != Qt::SolidLine)
    {
        buffer.append(",\"dash\":{");
        {
            buffer.append("\"offset\":").append(QString::number(p.dashOffset()));
            buffer.append(",\"pattern\":[");
            {
                foreach (qreal d, p.dashPattern())
                    buffer.append(QString::number(d)).append(',');
                if (buffer[buffer.length()-1] == ',')
                    buffer.remove(buffer.length()-1, 1);
            }
            buffer.append("]");
        }
        buffer.append("}");
    }

    buffer.append(",\"cap\":").append(QString::number(p.capStyle()));
    buffer.append(",\"join\":").append(QString::number(p.joinStyle()));
    if (p.joinStyle() == Qt::MiterJoin)
        buffer.append(",\"miter\":").append(QString::number(p.miterLimit()));
    buffer.append(",\"width\":").append(QString::number(p.widthF()));
    buffer.append('}');
}

void JSONBuilder::brush(const QBrush & b)
{

    buffer.append("\"brush\":{")
          .append("\"style\":").append(QString::number(b.style()))
          .append(",");

    switch (b.style())
    {
        case Qt::NoBrush:
            break;
        case Qt::SolidPattern:
            color(b.color());
            break;
        case Qt::LinearGradientPattern:
        case Qt::RadialGradientPattern:
        case Qt::ConicalGradientPattern:
            if (b.gradient())
                gradient(*(b.gradient()));
            break;
        case Qt::TexturePattern:
            {
                QImage img = b.textureImage();
                if (!img.isNull())
                {
                    QByteArray byteArray;
                    QBuffer buf(&byteArray);
                    img.save(&buf, "PNG");
                    buffer.append("\"texture\":\"data:image/png;base64,").append(byteArray.toBase64()).append("\"");
                }
                else
                {
                    QPixmap pix = b.texture();
                    if (!pix.isNull())
                    {
                        QByteArray byteArray;
                        QBuffer buf(&byteArray);
                        pix.save(&buf, "PNG");
                        buffer.append("\"texture\":\"data:image/png;base64,").append(byteArray.toBase64()).append("\"");
                    }
                }
            }
            break;
        default:
            color(b.color());
            break;
    }

    if (b.style() != Qt::NoBrush && !b.transform().isIdentity())
        transform(b.transform());

    if (buffer[buffer.length()-1] == ',')
        buffer.remove(buffer.length()-1, 1);

    buffer.append('}');
}

void JSONBuilder::bbrush(const QBrush & b)
{

    buffer.append("\"bbrush\":{")
          .append("\"style\":").append(QString::number(b.style()))
          .append(",");

    switch (b.style())
    {
        case Qt::NoBrush:
            break;
        case Qt::SolidPattern:
            color(b.color());
            break;
        case Qt::LinearGradientPattern:
        case Qt::RadialGradientPattern:
        case Qt::ConicalGradientPattern:
            if (b.gradient())
                gradient(*(b.gradient()));
            break;
        case Qt::TexturePattern:
            {
                QImage img = b.textureImage();
                if (!img.isNull())
                {
                    QByteArray byteArray;
                    QBuffer buf(&byteArray);
                    img.save(&buf, "PNG");
                    buffer.append("\"image\":\"data:image/png;base64,").append(byteArray.toBase64()).append("\"");
                }
                else
                {
                    QPixmap pix = b.texture();
                    if (!pix.isNull())
                    {
                        QByteArray byteArray;
                        QBuffer buf(&byteArray);
                        pix.save(&buf, "PNG");
                        buffer.append("\"image\":\"data:image/png;base64,").append(byteArray.toBase64()).append("\"");
                    }
                }
            }
            break;
        default:
            color(b.color());
            break;
    }

    if (b.style() != Qt::NoBrush && !b.transform().isIdentity())
        transform(b.transform());

    if (buffer[buffer.length()-1] == ',')
        buffer.remove(buffer.length()-1, 1);

    buffer.append('}');
}

void JSONBuilder::gradient(const QGradient & g)
{
    buffer.append("\"gradient\":{");
    buffer.append("\"type\":").append(QString::number(g.type()).toAscii());
    buffer.append(",\"stops\":[");
    {
        foreach (QGradientStop stop, g.stops())
            buffer.append("[")
                    .append(QString::number(stop.first)).append(",\"")
                    .append(stop.second.name()).append("\"],");
    }
    if (buffer[buffer.length()-1] == ',')
        buffer.remove(buffer.length()-1, 1);
    buffer.append("],\"spread\":").append(QString::number(g.spread()).toAscii());
    buffer.append(",\"mode\":").append(QString::number(g.coordinateMode()).toAscii());
    buffer.append("}");
}

void JSONBuilder::transform(const QTransform & t)
{
    buffer.append("\"transform\":[[")
          .append(QString::number(t.m11())).append(",")
          .append(QString::number(t.m12())).append(",")
          .append(QString::number(t.m13())).append("],[")
          .append(QString::number(t.m21())).append(",")
          .append(QString::number(t.m22())).append(",")
          .append(QString::number(t.m23())).append("],[")
          .append(QString::number(t.m31())).append(",")
          .append(QString::number(t.m32())).append(",")
            .append(QString::number(t.m33())).append("]]");
}
