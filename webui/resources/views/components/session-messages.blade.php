{{-- Displays session messages as alerts --}}
@if(flash()->message)
    <div class="notification {{ flash()->class }}">
        {{ flash()->message }}
    </div>
@endif
