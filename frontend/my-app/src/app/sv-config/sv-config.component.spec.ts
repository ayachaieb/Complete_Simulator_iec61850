import { ComponentFixture, TestBed } from '@angular/core/testing';

import { SvConfigComponent } from './sv-config.component';

describe('SvConfigComponent', () => {
  let component: SvConfigComponent;
  let fixture: ComponentFixture<SvConfigComponent>;

  beforeEach(async () => {
    await TestBed.configureTestingModule({
      imports: [SvConfigComponent]
    })
    .compileComponents();

    fixture = TestBed.createComponent(SvConfigComponent);
    component = fixture.componentInstance;
    fixture.detectChanges();
  });

  it('should create', () => {
    expect(component).toBeTruthy();
  });
});
