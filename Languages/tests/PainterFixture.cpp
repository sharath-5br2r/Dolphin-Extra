// Not compiled. It exists so the string-extraction test can prove that adding
// the two `translate` keywords does not pull in QPainter::translate calls.

void Draw(QPainter& p, int width, int height)
{
  p.translate(0, height);
  p.translate(width / 2.0, height / 2.0);
}
