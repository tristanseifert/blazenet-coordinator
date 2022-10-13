#ifndef GUI_TEXTRENDERER_H
#define GUI_TEXTRENDERER_H

#include <cairo.h>
#include <pango/pangocairo.h>

#include <fmt/format.h>

#include <stdexcept>
#include <string_view>
#include <utility>

namespace Gui {
/**
 * @brief Helper for rendering text
 */
class TextRenderer {
    public:
        /// Horizontal text alignment
        enum class HorizontalAlign {
            Left,
            Center,
            Right,
        };

        /// Vertical text alignment
        enum class VerticalAlign {
            Top,
            Middle,
            Bottom
        };

        /**
         * @brief Ellipsization mode
         *
         * Ellipsization is the process of inserting an ellipsis character (…) in a string of text that is
         * too large to fit in the alotted space.
         */
        enum class EllipsizeMode {
            /// Do not insert an ellipsis anywhere
            None,
            /// Omit characters at the beginning of the text
            Start,
            /// Omit characters in the middle of the text
            Middle,
            /// Omit characters at the end of the text
            End,
        };

    public:
        /**
         * @brief Initialize text renderer
         */
        TextRenderer(cairo_t *ctx) {
            this->layout = pango_cairo_create_layout(ctx);

            this->setTextLayoutEllipsization(EllipsizeMode::Middle);
            this->setTextLayoutWrapMode(false, true);
        }

        /**
         * @brief Clean up text rendering resources
         */
        ~TextRenderer() {
            if(this->fontDesc) {
                pango_font_description_free(this->fontDesc);
            }
            g_object_unref(this->layout);
        }


        /**
         * @brief Update the text ellipsization mode of the text drawing context
         *
         * @param newMode Text ellipsization mode to set
         */
        void setTextLayoutEllipsization(const EllipsizeMode newMode) {
            switch(newMode) {
                case EllipsizeMode::None:
                    pango_layout_set_ellipsize(this->layout, PANGO_ELLIPSIZE_NONE);
                    break;
                case EllipsizeMode::Start:
                    pango_layout_set_ellipsize(this->layout, PANGO_ELLIPSIZE_START);
                    break;
                case EllipsizeMode::Middle:
                    pango_layout_set_ellipsize(this->layout, PANGO_ELLIPSIZE_MIDDLE);
                    break;
                case EllipsizeMode::End:
                    pango_layout_set_ellipsize(this->layout, PANGO_ELLIPSIZE_END);
                    break;
            }
        }

        /**
         * @brief Update the wrapping and line break mode
         *
         * @param multiParagraph Whether the text renderer renders multiple paragraphs
         * @param wordWrap Whether lines are wrapped on word (`true`) or character (`false`) boundaries
         */
        void setTextLayoutWrapMode(const bool multiParagraph, const bool wordWrap) {
            if(wordWrap) {
                pango_layout_set_wrap(this->layout, PANGO_WRAP_WORD);
            } else {
                pango_layout_set_wrap(this->layout, PANGO_WRAP_CHAR);
            }
            pango_layout_set_single_paragraph_mode(this->layout, !multiParagraph);
        }

        /**
         * @brief Update the text alignment and justification settings of the text layout context
         *
         * @param newAlign New horizontal alignment setting
         * @param justified Whether text is justified
         */
        void setTextLayoutAlign(const HorizontalAlign newAlign, const bool justified = false) {
            switch(newAlign) {
                case HorizontalAlign::Left:
                    pango_layout_set_alignment(this->layout, PANGO_ALIGN_LEFT);
                    break;
                case HorizontalAlign::Center:
                    pango_layout_set_alignment(this->layout, PANGO_ALIGN_CENTER);
                    break;
                case HorizontalAlign::Right:
                    pango_layout_set_alignment(this->layout, PANGO_ALIGN_RIGHT);
                    break;
            }

            pango_layout_set_justify(this->layout, justified);
        }

        /**
         * @brief Set the new font
         */
        void setFont(const std::string_view name, const double size) {
            if(this->fontDesc) {
                pango_font_description_free(this->fontDesc);
            }

            this->fontDesc = this->getFont(name, size);
            this->fontDirty = true;
        }

        /**
         * @brief Render a string
         */
        void draw(cairo_t *ctx, const std::pair<double, double> origin,
                const std::pair<double, double> size, const std::tuple<double, double, double> color,
                const std::string_view &data, const HorizontalAlign halign = HorizontalAlign::Center,
                const VerticalAlign valign = VerticalAlign::Middle, const bool justify = false,
                const bool withTags = false) {
            int width, height;
            double pX, pY;

            // set string content
            this->setTextContent(data, withTags);
            this->setTextLayoutAlign(halign, justify);

            if(this->fontDirty) {
                pango_layout_set_font_description(this->layout, this->fontDesc);
                this->fontDirty = false;
            }

            // render
            cairo_move_to(ctx, origin.first, origin.second);

            // lay out the text and get its size
            pango_layout_set_width(this->layout, size.first * PANGO_SCALE);
            pango_layout_set_height(this->layout, size.second * PANGO_SCALE);

            pango_cairo_update_layout(ctx, this->layout);

            pango_layout_get_size(this->layout, &width, &height);

            // perform vertical align offsetting
            cairo_get_current_point(ctx, &pX, &pY);

            switch(valign) {
                case VerticalAlign::Middle:
                    cairo_move_to(ctx, pX, pY);
                    pY += (size.second - (height / PANGO_SCALE)) / 2;
                    cairo_move_to(ctx, pX, pY);
                    break;

                case VerticalAlign::Bottom:
                    pY += size.second - (height / PANGO_SCALE);
                    cairo_move_to(ctx, pX, pY);
                    break;

                default:
                    break;
            }

            // render it
            const auto [r, g, b] = color;
            cairo_set_source_rgb(ctx, r, g, b);
            pango_cairo_show_layout(ctx, this->layout);
        }

    protected:
        /**
         * @brief Parse a font descriptor string
         *
         * Fonts are automatically loaded using the system's font discovery mechanism. Names are parsed as
         * [Pango FontDescriptions](https://docs.gtk.org/Pango/type_func.FontDescription.from_string.html)
         * so you can customize the style, variants, weight, gravity, and stretch values of the font.
         *
         * @param name Font name
         * @param size Font size, in points
         */
        PangoFontDescription *getFont(const std::string_view name, const double size) const {
            auto desc = pango_font_description_from_string(name.data());
            pango_font_description_set_size(desc, size * PANGO_SCALE);

            return desc;
        }

        /**
         * @brief Set the text content of the text layout context
         *
         * Update the string content that will be drawn by the layout context. If specified, the text can
         * be parsed for attributes which affect how it is rendered; this is implemented by Pango, see
         * [this page](https://docs.gtk.org/Pango/pango_markup.html) for documentation on the markup
         * format.
         *
         * @param str String to set
         * @param parseMarkup Whether Pango markup should be parsed
         *
         * @remark Note that when markup is parsed, any existing attributes are replaced.
         */
        void setTextContent(const std::string_view &str, const bool parseMarkup) {
            if(!parseMarkup) {
                pango_layout_set_text(this->layout, str.data(), str.length());
            } else {
                PangoAttrList *attrList{nullptr};
                char *strippedStr{nullptr};
                GError *outError{nullptr};

                // parse markup
                auto ret = pango_parse_markup(str.data(), str.length(), 0, &attrList, &strippedStr,
                        nullptr, &outError);
                if(!ret) {
                    if(outError) {
                        throw std::runtime_error(fmt::format("pango_parse_markup failed: {} ({})",
                                    outError->message, outError->code));
                    } else {
                        throw std::runtime_error("unspecified error in pango_parse_markup");
                    }
                }

                // apply text and attributes
                pango_layout_set_text(this->layout, strippedStr, -1);
                pango_layout_set_attributes(this->layout, attrList);

                // clean up
                free(strippedStr);
                pango_attr_list_unref(attrList);
            }
        }

    private:
        /// Text layout object
        PangoLayout *layout{nullptr};

        /// Font descriptor
        PangoFontDescription *fontDesc{nullptr};
        /// Whether font has been changed
        bool fontDirty{false};
};
}

#endif
