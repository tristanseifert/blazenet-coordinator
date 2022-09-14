/*
 * Implements support for the Bulma tab component.
 *
 * We'll attach click listeners to every <a> tag inside a .tab div. Each of these links should have
 * as its href the id of the tab page to show. When a tab is clicked, all other pages in the tab
 * container are hidden.
 *
 * The tab container's id is specified by the data-tab-container attribute on the .tabs.
 */
import $ from 'cash-dom';

$(document).ready(() => {
    // XXX: will it always be a div?
    const tabs = $('div.tabs');
    for(var i = 0; i < tabs.length; i++) {
        const tabBar = $(tabs[i]);
        const tabContainer = $(tabBar.data('tab-container'));

        // add click listeners
        const tabLinks = tabBar.find('a');
        for(var j = 0; j < tabLinks.length; j++) {
            const tabLink = $(tabLinks[j]);
            const pageId = tabLink.attr('href');

            tabLink.on('click', () => {
                // update visible page
                tabContainer.children().addClass('is-hidden');
                $(pageId).removeClass('is-hidden');

                // update active tab
                tabBar.find('li').removeClass('is-active');
                tabBar.find('li').attr('aria-selected', 'false');
                tabLink.parent().addClass('is-active');
                tabLink.parent().attr('aria-selected', 'true');

                return false;
            });
        }
    }
});
