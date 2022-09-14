/*
 * Basic support for modals.
 *
 * We implement the close handler (via the `modal-close` class) and triggers that open modals when
 * clicking on a button. It's also optimized to handle the "delete" button in card modal's headers,
 * and buttons with the `is-closer` class in footers.
 */
import $, { Cash } from 'cash-dom';

$(document).ready(() => {
    // wrapper element for the image modal (dynamically added)
    var imageModal: Cash|null = null;

    // modal openers
    $('.modal-trigger').on('click', function(this: HTMLElement) {
        $($(this).data('target')).addClass('is-active');
        return false;
    });

    // modal closers
    const closeModal = function(this: HTMLElement) {
        const modal = $(this).closest('.modal');
        modal.removeClass('is-active');
        return false;
    };

    $('.modal-background, .modal-close, .modal-card-head .delete, .modal-card-foot .is-closer')
    .on('click', closeModal);

    // close all modals on escape
    document.addEventListener('keydown', (event: KeyboardEvent) => {
        const e = event || window.event;
        if(e.keyCode === 27) {
            // do not dismiss modals with the no-escape-close class
            $('.modal:not(.no-escape-close)').removeClass('is-active');
        }
    });

    // image modals
    $('a.image-lightbox').on('click', function(this: HTMLAnchorElement) {
        // insert the wrapper to body
        if(!imageModal) {
            imageModal = $(`<div class="modal lightbox">
  <div class="modal-background"></div>
  <div class="modal-content">
    <figure class="image">
      <img src="" class="image">
    </figure>
  </div>
  <button class="modal-close is-large" aria-label="Close"></button>
</div>`);
            $('body').append(imageModal).find('.modal-close').on('click', closeModal);
        }

        // update its image and info
        const imageUrl = $(this).attr('href');
        if(!imageUrl) {
            console.error('invalid href on image modal opener!', this);
            return;
        }

        imageModal.find('img').attr('src', imageUrl);

        const imageTitle = $(this).attr('title');
        if(imageTitle) {
            imageModal.find('img').attr('alt', imageTitle);
        } else {
            imageModal.find('img').removeAttr('alt');
        }

        // show the wrapper
        imageModal.addClass('is-active');

        return false;
    });
});
