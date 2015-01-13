#ifndef XMLESCAPE_HH
#define XMLESCAPE_HH

/*!
 * \addtogroup views
 */
/*!@{*/

/*!
 * Little helper class for buffer-less escaping of data for XML character data
 * while generating XML.
 */
class XmlEscape
{
  public:
    const char *const src_;

    XmlEscape(const XmlEscape &) = delete;
    XmlEscape &operator=(const XmlEscape &) = delete;

    explicit XmlEscape(const char *src): src_(src) {}
    explicit XmlEscape(const std::string &src): src_(src.c_str()) {}
};

/*!
 * Escape XML character data on the fly.
 */
static std::ostream &operator<<(std::ostream &os, const XmlEscape &data)
{
    size_t i = 0;

    while(1)
    {
        const char ch = data.src_[i++];

        if(ch == '\0')
            break;

        if(ch == '&')
            os << "&amp;";
        else if(ch == '<')
            os << "&lt;";
        else if(ch == '>')
            os << "&gt;";
        else if(ch == '\'')
            os << "&apos;";
        else if(ch == '"')
            os << "&quot;";
        else
            os << ch;
    }

    return os;
}

/*!@}*/

#endif /* !XMLESCAPE_HH */
